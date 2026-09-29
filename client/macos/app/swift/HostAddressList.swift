import SwiftUI

struct HostAddressList: View {
    let addresses: [LocalAddress]
    var staleIp: String?

    var body: some View {
        if addresses.isEmpty {
            if let staleIp {
                Text("\(staleIp)  (\(DeskhubClient.string(DHStrBindNotConnectedNote)))")
                    .foregroundStyle(.secondary)
            } else {
                Text(DeskhubClient.string(DHStrNoNetworkAddress)).foregroundStyle(.secondary)
            }
        } else {
            ForEach(addresses) { addr in
                HStack(spacing: 14) {
                    Text(addr.name).frame(width: 150, alignment: .leading).lineLimit(1)
                    Text(addr.ip).fontWeight(.bold).textSelection(.enabled)
                    Spacer(minLength: 0)
                    CopyButton { addr.ip }
                        .frame(width: 84)
                }
            }
        }
    }
}
