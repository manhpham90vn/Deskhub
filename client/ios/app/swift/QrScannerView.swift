import AVFoundation
import SwiftUI
import UIKit

enum CameraAccess {
    static func request() async -> Bool {
        await AVCaptureDevice.requestAccess(for: .video)
    }
}

struct QrScannerView: View {
    let onInvite: @MainActor (String) -> Void

    @Environment(\.dismiss) private var dismiss
    @State private var cameraUnavailable = false

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                Text(DeskhubClient.string(DHStrScanQrAction))
                    .font(.system(size: iosHeadingSize, weight: .bold))
                    .foregroundStyle(DeskhubPalette.heading)
                    .frame(maxWidth: .infinity, alignment: .leading)
                SessionCloseButton { dismiss() }
            }
            .padding(iosPagePadding)

            Group {
                if cameraUnavailable {
                    iosError(DeskhubClient.string(DHStrCameraDenied))
                        .multilineTextAlignment(.center)
                        .padding(iosPagePadding)
                } else {
                    QrCameraView(onInvite: accept) { cameraUnavailable = true }
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            iosHint(DeskhubClient.string(DHStrQrHint))
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(iosPagePadding)
        }
        .background { DeskhubPalette.page.ignoresSafeArea() }
    }

    private func accept(_ invite: String) {
        dismiss()
        onInvite(invite)
    }
}

private struct QrCameraView: UIViewControllerRepresentable {
    let onInvite: @MainActor (String) -> Void
    let onDenied: @MainActor () -> Void

    func makeUIViewController(context _: Context) -> QrCameraController {
        let controller = QrCameraController()
        controller.onInvite = onInvite
        controller.onDenied = onDenied
        return controller
    }

    func updateUIViewController(_: QrCameraController, context _: Context) {}
}

private final class CaptureSessionBox: @unchecked Sendable {
    let session = AVCaptureSession()
}

final class QrCameraController: UIViewController, AVCaptureMetadataOutputObjectsDelegate {
    var onInvite: @MainActor (String) -> Void = { _ in }
    var onDenied: @MainActor () -> Void = {}

    private let box = CaptureSessionBox()
    private var preview: AVCaptureVideoPreviewLayer?
    private var delivered = false

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        guard AVCaptureDevice.authorizationStatus(for: .video) == .authorized else {
            onDenied()
            return
        }
        configureSession()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        preview?.frame = view.bounds
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        let capture = box
        Task.detached {
            if capture.session.isRunning { capture.session.stopRunning() }
        }
    }

    private func configureSession() {
        let session = box.session
        guard let camera = AVCaptureDevice.default(for: .video),
              let input = try? AVCaptureDeviceInput(device: camera),
              session.canAddInput(input)
        else {
            onDenied()
            return
        }
        let output = AVCaptureMetadataOutput()
        guard session.canAddOutput(output) else {
            onDenied()
            return
        }
        session.beginConfiguration()
        session.addInput(input)
        session.addOutput(output)
        output.setMetadataObjectsDelegate(self, queue: .main)
        output.metadataObjectTypes = [.qr]
        session.commitConfiguration()

        let layer = AVCaptureVideoPreviewLayer(session: session)
        layer.videoGravity = .resizeAspectFill
        layer.frame = view.bounds
        view.layer.addSublayer(layer)
        preview = layer

        let capture = box
        Task.detached { capture.session.startRunning() }
    }

    nonisolated func metadataOutput(
        _: AVCaptureMetadataOutput, didOutput objects: [AVMetadataObject],
        from _: AVCaptureConnection
    ) {
        let texts = objects.compactMap { ($0 as? AVMetadataMachineReadableCodeObject)?.stringValue }
        guard let invite = texts.first(where: DeskhubClient.isPairingInvite) else { return }
        MainActor.assumeIsolated {
            deliver(invite)
        }
    }

    private func deliver(_ invite: String) {
        guard !delivered else { return }
        delivered = true
        onInvite(invite)
    }
}
