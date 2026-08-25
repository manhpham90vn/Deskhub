import SwiftUI

struct ContentView: View {
    var sharing: SharingModel
    @State private var connect = ConnectModel()

    var body: some View {
        MainMenuView(connect: connect, sharing: sharing)
            .frame(minWidth: 720, minHeight: 620)
            .navigationTitle(DeskhubClient.string(DHStrAppTitle))
    }
}
