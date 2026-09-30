import SwiftUI

@main
struct DeskhubApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @State private var sharing = SharingModel()
    @State private var tray = TrayPresence()

    var body: some Scene {
        Window("Deskhub", id: "main") {
            ContentView(sharing: sharing)
                .onAppear {
                    NSApp.setActivationPolicy(.regular)
                }
                .onDisappear(perform: mainWindowClosed)
                .onChange(of: sharing.startHidden) { _, on in
                    if !on { tray.attachedForSession = false }
                }
        }
        .windowResizability(.contentMinSize)
        .defaultSize(width: 1240, height: 780)

        Window(DeskhubClient.string(DHStrQrWindowTitle), id: PairingQrShowButton.windowId) {
            PairingQrWindow(sharing: sharing)
        }
        .windowResizability(.contentSize)

        WindowGroup(id: "connection", for: ConnectionRequest.self) { $request in
            if let request {
                ConnectionWindow(request: request)
            }
        }
        .windowResizability(.contentSize)
        .defaultSize(width: 460, height: 300)

        WindowGroup(id: "viewer", for: ViewerRequest.self) { $request in
            if let request {
                ViewerWindow(request: request)
            }
        }
        .windowResizability(.contentMinSize)
        .defaultSize(width: 1024, height: 634)

        WindowGroup(id: "terminal", for: TerminalRequest.self) { $request in
            if let request {
                TerminalWindow(request: request)
            }
        }
        .windowResizability(.contentMinSize)
        .defaultSize(width: 900, height: 560)

        WindowGroup(id: "transfer", for: TransferRequest.self) { $request in
            if let request {
                FileSendWindow(request: request)
            }
        }
        .windowResizability(.contentMinSize)
        .defaultSize(width: 520, height: 460)

        WindowGroup(id: "localShell", for: UInt32.self) { $termId in
            if let termId {
                LocalShellWindow(termId: termId)
            }
        }
        .windowResizability(.contentMinSize)
        .defaultSize(width: 900, height: 560)

        MenuBarExtra(
            "Deskhub",
            systemImage: "rectangle.on.rectangle",
            isInserted: trayInserted
        ) {
            TrayMenu(sharing: sharing)
        }
    }

    private var trayInserted: Binding<Bool> {
        Binding(
            get: { sharing.startHidden || tray.attachedForSession },
            set: { inserted in
                guard !inserted else { return }
                tray.attachedForSession = false
                sharing.startHidden = false
            }
        )
    }

    private var keepsRunningWithoutMainWindow: Bool {
        sharing.startHidden || sharing.isSharing || tray.attachedForSession
    }

    private func mainWindowClosed() {
        guard !appDelegate.isTerminating else { return }
        guard keepsRunningWithoutMainWindow else {
            Task { @MainActor in NSApp.terminate(nil) }
            return
        }
        if sharing.isSharing { tray.attachedForSession = true }
        NSApp.setActivationPolicy(.accessory)
    }
}

private struct TerminalWindow: View {
    let request: TerminalRequest
    @State private var model = TerminalModel()
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        TerminalScreen(model: model)
            .navigationTitle(DeskhubClient.addressHost(request.address))
            .frame(minWidth: 640, idealWidth: 900, maxWidth: .infinity,
                   minHeight: 400, idealHeight: 560, maxHeight: .infinity)
            .task {
                if !model.open(address: request.address) {
                    dismiss()
                }
            }
            .onDisappear { model.stop() }
    }
}

private struct LocalShellWindow: View {
    let termId: UInt32
    @State private var model = TerminalModel()
    @Environment(\.dismiss) private var dismiss

    private var title: String { DeskhubClient.string(DHStrTerminalLocalWindowTitle) }

    var body: some View {
        TerminalScreen(model: model)
            .navigationTitle(title)
            .frame(minWidth: 640, idealWidth: 900, maxWidth: .infinity,
                   minHeight: 400, idealHeight: 560, maxHeight: .infinity)
            .task {
                guard dh_share_local_shell_alive(termId) else {
                    dismiss()
                    return
                }
                model.attach(LocalTerminalFeed(termId: termId))
            }
            .onDisappear { model.stop() }
    }
}

private struct TrayMenu: View {
    var sharing: SharingModel
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        Button(
            DeskhubClient.string(mainWindowShown ? DHStrTrayHideWindow : DHStrTrayShowWindow)
        ) {
            if mainWindowShown {
                hideMainWindow()
            } else {
                showMainWindow()
            }
        }
        Button(DeskhubClient.string(sharing.isSharing ? DHStrStopSharing : DHStrStartSharing)) {
            toggleSharing()
        }
        Divider()
        Button(DeskhubClient.string(DHStrTrayQuit)) { NSApp.terminate(nil) }
    }

    private var mainWindow: NSWindow? {
        NSApp.windows.first { $0.identifier?.rawValue.hasPrefix("main") == true }
    }

    private var mainWindowShown: Bool {
        mainWindow?.isVisible == true
    }

    private func hideMainWindow() {
        mainWindow?.close()
    }

    private func showMainWindow() {
        NSApp.setActivationPolicy(.regular)
        openWindow(id: "main")
        NSApp.activate(ignoringOtherApps: true)
    }

    private func toggleSharing() {
        if sharing.isSharing {
            sharing.stopSharing()
            return
        }
        Task {
            if await !sharing.startSharing() { showMainWindow() }
        }
    }
}
