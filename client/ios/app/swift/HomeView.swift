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
                DevicesPage()
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

            SettingsView(settings: model.settings) { port in
                model.discovery.usePort(port)
            }
            .tabItem {
                Label(DeskhubClient.string(DHStrSidebarSettings), systemImage: "gearshape")
            }
            .tag(3)
        }
    }
}
