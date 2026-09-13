import Foundation
import Observation

@MainActor @Observable
final class AppModel {
    var connect = ConnectModel()
    var discovery = DiscoveryModel()
    var settings = SettingsModel()
    var sharing = SharingModel()

    var screen: ClientRoute = .connect
    var sources: [Source] = []
    private(set) var stream: StreamModel?
    private(set) var terminal: TerminalModel?
    var fileSend: FileSendModel?

    func beginConnect(to address: String, passcode: String) {
        connect.address = address
        connect.passcode = passcode
        beginConnect()
    }

    func beginConnect() {
        guard !connect.isConnecting else { return }
        connect.saveDeviceName()
        Task {
            guard let found = await connect.connectAuth() else { return }
            await discovery.remember(
                address: connect.acceptedAddress, passcode: connect.acceptedPasscode
            )
            sources = found.sources
        }
    }

    var hasLiveSession: Bool {
        stream != nil || terminal != nil || fileSend != nil
    }

    func dropHost() {
        connect.forgetHost()
        sources = []
    }

    func openDesktop() {
        guard let found = connect.authed, !found.sources.isEmpty else { return }
        sources = found.sources
        let decision = DeskhubClient.connectDecision(found.sources)
        if decision.showPicker {
            screen = .sourcePicker(found.sources)
        } else {
            startStream(sourceId: decision.sourceId)
        }
    }

    func startStream(sourceId: UInt8) {
        connect.connectError = ""
        let address = connect.acceptedAddress
        let model = StreamModel(
            address: address, passcode: connect.acceptedPasscode, sourceId: sourceId,
            sourceName: sourceName(of: sourceId)
        )
        stream = model
        screen = .stream

        Task {
            await model.start()
            guard model.failedToStart, stream === model else { return }
            stream = nil
            screen = .connect
            connect.connectError = DeskhubClient.couldNotConnect(address) + " "
                + DeskhubClient.string(DHStrInvalidAddressHint)
        }
    }

    func switchSource(to sourceId: UInt8) {
        guard let stream else { return }
        let name = sourceName(of: sourceId)
        Task { await stream.switchSource(to: sourceId, name: name) }
    }

    func disconnect() {
        stream?.disconnect()
        stream = nil
        screen = .connect
    }

    func openFileSend() {
        guard connect.canOpenFiles else { return }
        connect.saveDeviceName()
        let sender = FileSendModel()
        sender.address = connect.acceptedAddress
        sender.passcode = connect.acceptedPasscode
        sender.deviceName = connect.deviceName
        fileSend = sender
    }

    func closeFileSend() {
        fileSend?.forgetTransfer()
        clearSendStaging()
        fileSend = nil
    }

    func openShell() {
        guard connect.canOpenShell else { return }
        connect.saveDeviceName()
        let address = connect.acceptedAddress
        let model = TerminalModel()
        guard model.open(address: address, passcode: connect.acceptedPasscode) else {
            connect.connectError = DeskhubClient.couldNotConnect(address)
            return
        }
        terminal = model
        screen = .terminal
    }

    func closeShell() {
        terminal?.stop()
        terminal = nil
        screen = .connect
    }

    private func sourceName(of sourceId: UInt8) -> String {
        sources.first { $0.id == sourceId }?.name ?? ""
    }
}
