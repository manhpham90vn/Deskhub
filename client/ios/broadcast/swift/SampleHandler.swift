import AVFoundation
import ReplayKit
import UIKit

final class SampleHandler: RPBroadcastSampleHandler, @unchecked Sendable {
    private static let statusInterval = DispatchTimeInterval.seconds(1)
    private static let errorDomain = "com.ios.deskhub.broadcast"
    private static let errorBufferBytes = 320
    private static let viewerNamesBufferBytes = 512

    private static let sampleRate = 48000.0
    private static let channels: AVAudioChannelCount = 2
    private static let frameShorts = 960 * 2

    private let statusQueue = DispatchQueue(label: "com.ios.deskhub.broadcast.status")
    private var statusTimer: DispatchSourceTimer?

    private var resampler: AudioResampler?
    private var pending = [Int16]()

    override func broadcastStarted(withSetupInfo _: [String: NSObject]?) {
        guard let container = BroadcastStatus.containerURL?.path else {
            finishBroadcastWithError(NSError(
                domain: SampleHandler.errorDomain,
                code: 2,
                userInfo: [NSLocalizedDescriptionKey: "Deskhub shared app container is unavailable"]
            ))
            return
        }
        dhb_start_broadcast(container, UIDevice.current.model)
        BroadcastStatus().save()
        startPublishingStatus()
    }

    override func broadcastFinished() {
        statusTimer?.cancel()
        dhb_finish_broadcast()
        BroadcastStatus.clear()
    }

    override func processSampleBuffer(
        _ sampleBuffer: CMSampleBuffer, with sampleBufferType: RPSampleBufferType
    ) {
        if sampleBufferType == .audioApp {
            forwardAudio(sampleBuffer)
            return
        }

        guard sampleBufferType == .video,
              let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer)
        else {
            return
        }

        dhb_push_frame(
            Unmanaged.passUnretained(pixelBuffer).toOpaque(),
            sampleOrientation(sampleBuffer)
        )
    }

    private func forwardAudio(_ sampleBuffer: CMSampleBuffer) {
        guard let source = AVAudioPCMBuffer.deskhubBuffer(from: sampleBuffer),
              let target = shared(for: source.format),
              let converted = target.render(source)
        else {
            return
        }

        pending.append(contentsOf: converted)
        while pending.count >= SampleHandler.frameShorts {
            pending.withUnsafeBufferPointer { buffer in
                dhb_push_audio(buffer.baseAddress, Int32(SampleHandler.frameShorts))
            }
            pending.removeFirst(SampleHandler.frameShorts)
        }
    }

    private func shared(for input: AVAudioFormat) -> AudioResampler? {
        if let existing = resampler, existing.matches(input) { return existing }
        guard let output = AVAudioFormat(
            commonFormat: .pcmFormatInt16,
            sampleRate: SampleHandler.sampleRate,
            channels: SampleHandler.channels,
            interleaved: true
        ) else {
            return nil
        }
        resampler = AudioResampler(from: input, to: output)
        return resampler
    }

    private func sampleOrientation(_ sampleBuffer: CMSampleBuffer) -> UInt32 {
        let value = CMGetAttachment(
            sampleBuffer,
            key: RPVideoSampleOrientationKey as CFString,
            attachmentModeOut: nil
        )
        return (value as? NSNumber)?.uint32Value ?? 1
    }

    private func startPublishingStatus() {
        let timer = DispatchSource.makeTimerSource(queue: statusQueue)
        timer.schedule(deadline: .now(), repeating: SampleHandler.statusInterval)
        timer.setEventHandler { [weak self] in self?.publishStatus() }
        statusTimer = timer
        timer.resume()
    }

    private func publishStatus() {
        let failure = startFailure()
        BroadcastStatus(
            sharing: dhb_sharing(),
            viewers: Int(dhb_viewer_count()),
            viewerNames: viewerNames(),
            memoryMB: Int(dhb_memory_footprint_mb()),
            error: failure
        ).save()

        guard !failure.isEmpty else { return }
        statusTimer?.cancel()
        dhb_finish_broadcast()
        finishBroadcastWithError(
            NSError(
                domain: SampleHandler.errorDomain,
                code: 1,
                userInfo: [NSLocalizedDescriptionKey: failure]
            )
        )
    }

    private func startFailure() -> String {
        var buffer = [CChar](repeating: 0, count: SampleHandler.errorBufferBytes)
        let written = Int(dhb_last_error(&buffer, Int32(buffer.count)))
        guard written > 0 else { return "" }
        let bytes = buffer.prefix(written).map { UInt8(bitPattern: $0) }
        return String(bytes: bytes, encoding: .utf8) ?? ""
    }

    private func viewerNames() -> String {
        var buffer = [CChar](repeating: 0, count: SampleHandler.viewerNamesBufferBytes)
        let written = Int(dhb_viewer_list(&buffer, Int32(buffer.count)))
        guard written > 0 else { return "" }
        let bytes = buffer.prefix(written).map { UInt8(bitPattern: $0) }
        return String(bytes: bytes, encoding: .utf8) ?? ""
    }
}
