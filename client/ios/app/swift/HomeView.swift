import SwiftUI

struct HomeView: View {
    @Bindable var model: AppModel
    @State private var page = StartPage.index() ?? 0

    var body: some View {
        TabView(selection: $page) {
            ConnectView(model: model)
                .tabItem {
                    Label(DeskhubClient.string(DHStrSidebarClient), systemImage: "display")
                }
                .tag(0)

            SharingView(model: model.sharing)
                .tabItem {
                    Label(
                        DeskhubClient.string(DHStrSidebarHost),
                        systemImage: "rectangle.on.rectangle"
                    )
                }
                .tag(1)

            ScrollView {
                DevicesPage(
                    trustedHostsRevision: model.connect.trustedHostsRevision,
                    onConnectHost: connectSavedHost
                )
                .padding()
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .tabItem {
                Label(
                    DeskhubClient.string(DHStrSidebarDevices),
                    systemImage: "checkmark.shield"
                )
            }
            .tag(2)

            SettingsView(settings: model.settings)
                .tabItem {
                    Label(DeskhubClient.string(DHStrSidebarSettings), systemImage: "gearshape")
                }
                .tag(3)
        }
        .onOpenURL(perform: openInvite)
    }

    private func connectSavedHost(_ address: String) {
        page = 0
        model.beginConnect(to: address)
    }

    private func openInvite(_ url: URL) {
        let invite = url.absoluteString
        guard DeskhubClient.isPairingInvite(invite) else {
            model.connect.connectError = DeskhubClient.string(DHStrInviteInvalid)
            return
        }
        page = 0
        model.beginConnect(invite: invite)
    }
}
