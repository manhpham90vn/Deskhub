import SwiftUI

struct DeviceTable: View {
    private static let headerHeight: CGFloat = 28
    private static let rowHeight: CGFloat = 24

    let rows: [DeviceListRow]
    let onPick: (DeviceListRow) -> Void

    @State private var selection: DeviceListRow.ID?

    var body: some View {
        Table(rows, selection: $selection) {
            TableColumn(DeskhubClient.string(DHStrDeviceNameLabel)) { row in
                Text(row.name.isEmpty ? "-" : row.name)
            }
            .width(180)
            TableColumn(DeskhubClient.string(DHStrHostAddressLabel)) { Text($0.addr) }.width(170)
            TableColumn("Last connected") { Text($0.lastConnected) }.width(150)
        }
        .contextMenu(forSelectionType: DeviceListRow.ID.self) { _ in
        } primaryAction: { picked in
            guard let id = picked.first, let row = rows.first(where: { $0.id == id })
            else { return }
            onPick(row)
        }
        .frame(height: height)
    }

    private var height: CGFloat {
        let content = DeviceTable.headerHeight + CGFloat(rows.count) * DeviceTable.rowHeight + 2
        return max(DeskhubListMetrics.minHeight, content)
    }
}
