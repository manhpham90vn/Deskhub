import SwiftUI

struct TransferRequest: Codable, Hashable {
    var address: String
    var passcode: String
    var name: String
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
                sender.passcode = request.passcode
                sender.deviceName = request.name
            }
            .onDisappear { sender.forgetTransfer() }
    }
}
