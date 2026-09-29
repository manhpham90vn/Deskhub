import SwiftUI

struct TransferRequest: Codable, Hashable {
    var address: String
}

struct FileSendWindow: View {
    @State private var sender = FileSendModel()
    private let request: TransferRequest

    init(request: TransferRequest) {
        self.request = request
    }

    var body: some View {
        FileSendView(model: sender)
            .navigationTitle(DeskhubClient.string(DHStrTransferSendHeading))
            .navigationSubtitle(request.address)
            .onAppear {
                sender.address = request.address
            }
            .onDisappear { sender.forgetTransfer() }
    }
}
