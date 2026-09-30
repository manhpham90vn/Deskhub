import SwiftUI
import UIKit

private let termFontSize: CGFloat = 13
private let termBackground = Color(red: 0x10 / 255.0, green: 0x12 / 255.0, blue: 0x18 / 255.0)
private let termCursor = Color(white: 0xE0 / 255.0).opacity(0.6)
private let termReattaching = Int32(DHTermReattaching.rawValue)
private let termRefused = Int32(DHTermRefused.rawValue)
private let termFailed = Int32(DHTermFailed.rawValue)
private let termAttrBold: UInt8 = 1
private let termAttrUnderline: UInt8 = 8

private var termCellSize: CGSize {
    let font = UIFont.monospacedSystemFont(ofSize: termFontSize, weight: .regular)
    let measured = ("M" as NSString).size(withAttributes: [.font: font])
    return CGSize(width: ceil(measured.width), height: ceil(measured.height))
}

private func termColor(_ red: UInt8, _ green: UInt8, _ blue: UInt8) -> Color {
    Color(red: Double(red) / 255.0, green: Double(green) / 255.0, blue: Double(blue) / 255.0)
}

extension TerminalModel {
    func openReportingFailure(address: String) {
        guard !open(address: address) else { return }
        state = termFailed
        message = DeskhubClient.couldNotConnect(address)
    }
}

struct IosTerminalScreen: View {
    @Bindable var model: TerminalModel
    let address: String
    let onClose: () -> Void

    @State private var keyboardOn = false

    var body: some View {
        Group {
            if model.showingPicker {
                IosShellPickerView(model: model, onClose: onClose)
            } else {
                session
            }
        }
        .background(Color.black.ignoresSafeArea())
    }

    private var session: some View {
        VStack(spacing: 0) {
            ZStack(alignment: .topLeading) {
                IosTerminalGridView(model: model, keyboardOn: $keyboardOn)

                if model.state == termReattaching {
                    SessionBanner(text: model.message)
                }

                SessionCloseButton(action: onClose)
                    .padding(12)
                    .frame(maxWidth: .infinity, alignment: .topTrailing)

                if model.state >= termRefused {
                    failureOverlay
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            IosTerminalExtraKeys(model: model) { keyboardOn.toggle() }

            Text(statusLine)
                .font(.caption)
                .foregroundStyle(.white.opacity(0.7))
                .lineLimit(1)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 12)
                .padding(.vertical, 4)
        }
        .onDisappear { keyboardOn = false }
    }

    private var statusLine: String {
        model.message.isEmpty ? DeskhubClient.string(DHStrTerminalExtraKeysHint) : model.message
    }

    private var failureOverlay: some View {
        VStack(spacing: 12) {
            Text(model.message)
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
            if model.state == termFailed {
                SessionTextButton("Retry") { model.openReportingFailure(address: address) }
            }
            SessionTextButton("Back", action: onClose)
        }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(sessionOverlayDim)
        .contentShape(Rectangle())
        .onTapGesture(perform: onClose)
    }
}

private struct IosTerminalGridView: View {
    @Bindable var model: TerminalModel
    @Binding var keyboardOn: Bool
    @State private var dragCarry: CGFloat = 0

    var body: some View {
        GeometryReader { proxy in
            let cell = termCellSize
            ZStack(alignment: .topLeading) {
                Canvas { context, size in
                    context.fill(
                        Path(CGRect(origin: .zero, size: size)), with: .color(termBackground)
                    )
                    drawCells(in: &context, cell: cell)
                }

                IosTerminalKeyInput(model: model, active: $keyboardOn)
                    .frame(width: 1, height: 1)
                    .opacity(0)
                    .allowsHitTesting(false)
            }
            .frame(width: proxy.size.width, height: proxy.size.height)
            .contentShape(Rectangle())
            .onTapGesture { keyboardOn = true }
            .gesture(scrollDrag(cell: cell))
            .onAppear { resize(to: proxy.size, cell: cell) }
            .onChange(of: proxy.size) { _, size in resize(to: size, cell: cell) }
        }
    }

    private func drawCells(in context: inout GraphicsContext, cell: CGSize) {
        let grid = model.grid
        for row in 0 ..< grid.rows {
            for col in 0 ..< grid.cols {
                let data = grid.cell(row, col)
                let origin = CGPoint(x: CGFloat(col) * cell.width, y: CGFloat(row) * cell.height)
                context.fill(
                    Path(CGRect(origin: origin, size: cell)),
                    with: .color(termColor(data.bgR, data.bgG, data.bgB))
                )
                guard data.codepoint != 32, let scalar = Unicode.Scalar(data.codepoint) else {
                    continue
                }
                let bold = data.attrs & termAttrBold != 0
                let underlined = data.attrs & termAttrUnderline != 0
                let glyph = Text(String(Character(scalar)))
                    .font(.system(size: termFontSize, weight: bold ? .bold : .regular, design: .monospaced))
                    .underline(underlined)
                    .foregroundStyle(termColor(data.fgR, data.fgG, data.fgB))
                context.draw(glyph, at: origin, anchor: .topLeading)
            }
        }
        guard grid.cursorVisible, grid.rows > 0 else { return }
        let origin = CGPoint(
            x: CGFloat(grid.cursorCol) * cell.width, y: CGFloat(grid.cursorRow) * cell.height
        )
        context.fill(Path(CGRect(origin: origin, size: cell)), with: .color(termCursor))
    }

    private func scrollDrag(cell: CGSize) -> some Gesture {
        DragGesture(minimumDistance: 10)
            .onChanged { value in
                let rows = Int((value.translation.height - dragCarry) / max(1, cell.height))
                guard rows != 0 else { return }
                dragCarry += CGFloat(rows) * cell.height
                model.scrollBy(rows: rows)
            }
            .onEnded { _ in dragCarry = 0 }
    }

    private func resize(to size: CGSize, cell: CGSize) {
        guard size.width > 0, size.height > 0 else { return }
        model.resize(
            cols: Int(size.width / max(1, cell.width)),
            rows: Int(size.height / max(1, cell.height))
        )
    }
}

private struct IosTerminalKeyInput: UIViewRepresentable {
    let model: TerminalModel
    @Binding var active: Bool

    func makeUIView(context _: Context) -> IosTermKeyView {
        let view = IosTermKeyView()
        view.model = model
        return view
    }

    func updateUIView(_ view: IosTermKeyView, context _: Context) {
        view.model = model
        let binding = $active
        view.onKeyboardDismissed = { binding.wrappedValue = false }
        if active, !view.isFirstResponder {
            view.becomeFirstResponder()
        } else if !active, view.isFirstResponder {
            view.resignFirstResponder()
        }
    }
}

final class IosTermKeyView: UIView, UIKeyInput {
    weak var model: TerminalModel?
    var onKeyboardDismissed: (() -> Void)?

    var keyboardType: UIKeyboardType = .asciiCapable
    var autocorrectionType: UITextAutocorrectionType = .no
    var spellCheckingType: UITextSpellCheckingType = .no
    var autocapitalizationType: UITextAutocapitalizationType = .none

    override init(frame: CGRect) {
        super.init(frame: frame)
        NotificationCenter.default.addObserver(
            self, selector: #selector(keyboardWillHide),
            name: UIResponder.keyboardWillHideNotification, object: nil
        )
    }

    @available(*, unavailable)
    required init?(coder _: NSCoder) { fatalError("init(coder:) is not supported") }

    @objc private func keyboardWillHide() {
        if isFirstResponder { onKeyboardDismissed?() }
    }

    override var canBecomeFirstResponder: Bool { true }

    var hasText: Bool { true }

    func insertText(_ text: String) {
        guard let model else { return }
        for scalar in text.unicodeScalars {
            model.typeChar(scalar.value)
        }
    }

    func deleteBackward() {
        model?.typeSpecial(TermKeyCode.backspace)
    }
}

private struct IosTerminalExtraKeys: View {
    @Bindable var model: TerminalModel
    let onKeyboard: () -> Void

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                key("Esc") { model.typeSpecial(TermKeyCode.escape) }
                key("Tab") { model.typeSpecial(TermKeyCode.tab) }
                key("Ctrl", active: model.latchCtrl) { model.latchCtrl.toggle() }
                key("Alt", active: model.latchAlt) { model.latchAlt.toggle() }
                key("\u{2190}") { model.typeSpecial(TermKeyCode.left) }
                key("\u{2193}") { model.typeSpecial(TermKeyCode.down) }
                key("\u{2191}") { model.typeSpecial(TermKeyCode.up) }
                key("\u{2192}") { model.typeSpecial(TermKeyCode.right) }
                key("^C") {
                    model.sendKey(TermKeyCode.char, codepoint: 99, ctrl: true)
                    model.latchCtrl = false
                    model.latchAlt = false
                }
                key("\u{2328}", action: onKeyboard)
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
        }
    }

    private func key(
        _ label: String, active: Bool = false, action: @escaping () -> Void
    ) -> some View {
        Button(label, action: action)
            .buttonStyle(
                .deskhubOutlined(
                    tint: active ? DeskhubPalette.accent : Color.white,
                    horizontalPadding: 12,
                    verticalPadding: 6
                )
            )
    }
}
