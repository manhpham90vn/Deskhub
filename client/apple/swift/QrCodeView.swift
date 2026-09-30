import SwiftUI

struct QrCodeView: View {
    private static let quietZoneModules = 4

    let modules: [[Bool]]

    var body: some View {
        Canvas { context, size in
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(.white))
            guard !modules.isEmpty else { return }
            let cells = CGFloat(modules.count + 2 * QrCodeView.quietZoneModules)
            let cell = min(size.width, size.height) / cells
            let originX = (size.width - cell * cells) / 2 + cell * CGFloat(QrCodeView.quietZoneModules)
            let originY = (size.height - cell * cells) / 2 + cell * CGFloat(QrCodeView.quietZoneModules)
            var dark = Path()
            for (rowIndex, row) in modules.enumerated() {
                for (columnIndex, isDark) in row.enumerated() where isDark {
                    dark.addRect(CGRect(
                        x: originX + CGFloat(columnIndex) * cell,
                        y: originY + CGFloat(rowIndex) * cell,
                        width: cell, height: cell
                    ))
                }
            }
            context.fill(dark, with: .color(.black))
        }
        .aspectRatio(1, contentMode: .fit)
    }
}
