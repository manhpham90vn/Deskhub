import SwiftUI

struct ContentView: View {
    @State private var model = AppModel()
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        Group {
            if let sender = model.fileSend {
                FileSendView(model: sender, subtitle: sender.address) { model.closeFileSend() }
            } else {
                routedScreen
            }
        }
        .preferredColorScheme(.dark)
        .pairingPrompt(FilesHost.shared.pairing)
        .onChange(of: scenePhase) { _, phase in
            switch phase {
            case .background:
                if model.hasLiveSession { BackgroundHold.shared.begin() }
            case .active:
                BackgroundHold.shared.end()
            default:
                break
            }
        }
        .task(id: scenePhase) {
            if scenePhase == .active {
                await FilesHost.shared.run()
            } else if scenePhase == .background {
                FilesHost.shared.stop()
            }
        }
    }

    @ViewBuilder private var routedScreen: some View {
        switch model.screen {
        case .connect, .sharing:
            HomeView(model: model)
        case let .sourcePicker(sources):
            SourcePickerView(model: model, sources: sources)
        case .stream:
            if let stream = model.stream {
                StreamView(session: model, model: stream)
            }
        case .terminal:
            if let terminal = model.terminal {
                TerminalScreen(
                    model: terminal,
                    title: DeskhubClient.addressHost(model.connect.address)
                ) { model.closeShell() }
            }
        }
    }
}
