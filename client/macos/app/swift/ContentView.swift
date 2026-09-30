import SwiftUI

struct ContentView: View {
    var sharing: SharingModel
    @State private var connect = ConnectModel()

    var body: some View {
        MainMenuView(connect: connect, sharing: sharing)
            .navigationTitle(DeskhubClient.string(DHStrAppTitle))
    }
}
