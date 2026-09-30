import SwiftUI

struct ContentView: View {
    var sharing: SharingModel
    @State private var connect = ConnectModel()

    var body: some View {
        MainMenuView(connect: connect, sharing: sharing)
            .frame(minWidth: 1000, minHeight: 640)
            .navigationTitle(DeskhubClient.string(DHStrAppTitle))
    }
}
