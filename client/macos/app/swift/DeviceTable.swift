import SwiftUI

struct DeviceTable: View {
    let rows: [DeviceListRow]
    let enabled: Bool
    let onPick: (DeviceListRow) -> Void

    @State private var selection: DeviceListRow.ID?

    var body: some View {
        Table(rows, selection: $selection) {
            TableColumn(DeskhubClient.string(DHStrDeviceNameLabel)) { Text($0.name) }.width(170)
            TableColumn("Address") { Text($0.addr) }.width(150)
            TableColumn("Last connected") { Text($0.lastConnected) }.width(150)
        }
        .frame(height: 130)
        .disabled(!enabled)
        .onChange(of: selection) { _, picked in
            guard let picked, let row = rows.first(where: { $0.id == picked })
            else { return }
            selection = nil
            onPick(row)
        }
    }
}
