import SwiftUI

#if os(macOS)
    import AppKit
#else
    import UIKit
#endif

struct TerminalRequest: Codable, Hashable {
    var address: String
    var passcode: String
}

private var termCellSize: CGSize {
    #if os(macOS)
        let font = NSFont.monospacedSystemFont(ofSize: 13, weight: .regular)
    #else
        let font = UIFont.monospacedSystemFont(ofSize: 13, weight: .regular)
    #endif
    let measured = ("M" as NSString).size(withAttributes: [.font: font])
    return CGSize(width: ceil(measured.width), height: ceil(measured.height))
}

private func termColor(_ red: UInt8, _ green: UInt8, _ blue: UInt8) -> Color {
    Color(
        red: Double(red) / 255.0, green: Double(green) / 255.0, blue: Double(blue) / 255.0
    )
}

private let termBackground = termColor(0x10, 0x12, 0x18)
private let termCursor = termColor(0xE0, 0xE0, 0xE0)

struct TerminalScreen: View {
    @Bindable var model: TerminalModel
    let title: String
    #if os(iOS)
        let onClose: () -> Void
    #endif

    var body: some View {
        VStack(spacing: 0) {
            #if os(iOS)
                TerminalGridView(model: model)
                    .overlay(alignment: .topTrailing) {
                        SessionCloseButton(action: onClose).padding(12)
                    }

                TerminalExtraKeys(model: model)
            #else
                TerminalGridView(model: model)
            #endif

            HStack(spacing: 12) {
                Text(model.message)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                Spacer(minLength: 0)
                if model.scrollOffset > 0 {
                    Button { model.scrollToBottom() } label: {
                        Text(verbatim: "\u{2193} \(model.scrollOffset)")
                            .font(.caption.monospacedDigit())
                    }
                }
            }
            .padding(8)
        }
        .background(termBackground)
        .trustPrompt(
            isPresented: $model.askingTrust,
            changed: model.trustChanged,
            fingerprint: model.trustFingerprint
        ) { model.answerTrust($0) }
    }
}

private struct TerminalGridView: View {
    @Bindable var model: TerminalModel
    @State private var dragCarry: CGFloat = 0

    var body: some View {
        GeometryReader { proxy in
            let cell = termCellSize
            ZStack(alignment: .topLeading) {
                Canvas { context, _ in
                    let grid = model.grid
                    for row in 0 ..< grid.rows {
                        for col in 0 ..< grid.cols {
                            let data = grid.cell(row, col)
                            let origin = CGPoint(
                                x: CGFloat(col) * cell.width, y: CGFloat(row) * cell.height
                            )
                            let rect = CGRect(origin: origin, size: cell)
                            context.fill(
                                Path(rect),
                                with: .color(termColor(data.bgR, data.bgG, data.bgB))
                            )
                            guard data.codepoint != 32,
                                  let scalar = Unicode.Scalar(data.codepoint)
                            else { continue }
                            let glyph = Text(String(Character(scalar)))
                                .font(.system(size: 13, design: .monospaced))
                                .foregroundColor(termColor(data.fgR, data.fgG, data.fgB))
                            context.draw(glyph, at: origin, anchor: .topLeading)
                        }
                    }
                    if grid.cursorVisible, grid.rows > 0 {
                        let origin = CGPoint(
                            x: CGFloat(grid.cursorCol) * cell.width,
                            y: CGFloat(grid.cursorRow) * cell.height
                        )
                        context.fill(
                            Path(CGRect(origin: origin, size: cell)),
                            with: .color(termCursor.opacity(0.6))
                        )
                    }
                }

                #if os(macOS)
                    TerminalKeyInput(model: model)
                #else
                    TerminalKeyInput(model: model)
                        .frame(width: 1, height: 1)
                #endif
            }
            .background(termBackground)
            .contentShape(Rectangle())
            .gesture(scrollDrag(cell: cell))
            .onAppear { resize(to: proxy.size, cell: cell) }
            .onChange(of: proxy.size) { _, size in resize(to: size, cell: cell) }
        }
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

#if os(macOS)

    private struct TerminalKeyInput: NSViewRepresentable {
        var model: TerminalModel

        func makeNSView(context _: Context) -> TermKeyView {
            let view = TermKeyView()
            view.model = model
            return view
        }

        func updateNSView(_ view: TermKeyView, context _: Context) {
            view.model = model
        }
    }

    final class TermKeyView: NSView {
        weak var model: TerminalModel?

        private var wheelCarry: CGFloat = 0

        override var acceptsFirstResponder: Bool { true }

        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            guard let window else { return }
            DispatchQueue.main.async { window.makeFirstResponder(self) }
        }

        override func scrollWheel(with event: NSEvent) {
            guard let model else { return }
            let step = event.hasPreciseScrollingDeltas ? termCellSize.height : 1
            wheelCarry += event.scrollingDeltaY
            let rows = Int((wheelCarry / max(1, step)).rounded(.towardZero))
            guard rows != 0 else { return }
            wheelCarry -= CGFloat(rows) * step
            model.scrollBy(rows: rows)
        }

        override func keyDown(with event: NSEvent) {
            guard let model else { return }
            if let key = TermKeyView.specialFor(event.keyCode) {
                model.sendKey(
                    key,
                    shift: event.modifierFlags.contains(.shift),
                    alt: event.modifierFlags.contains(.option),
                    ctrl: event.modifierFlags.contains(.control)
                )
                return
            }
            guard let typed = event.charactersIgnoringModifiers, !typed.isEmpty else { return }
            for scalar in typed.unicodeScalars where scalar.value >= 32 {
                if event.modifierFlags.contains(.control) {
                    model.sendKey(
                        TermKeyCode.char, codepoint: scalar.value,
                        shift: event.modifierFlags.contains(.shift),
                        alt: event.modifierFlags.contains(.option), ctrl: true
                    )
                } else {
                    model.sendKey(
                        TermKeyCode.char, codepoint: scalar.value,
                        alt: event.modifierFlags.contains(.option)
                    )
                }
            }
        }

        private static let specialKeys: [UInt16: Int32] = [
            36: TermKeyCode.enter, 76: TermKeyCode.enter,
            51: TermKeyCode.backspace,
            48: TermKeyCode.tab,
            53: TermKeyCode.escape,
            126: TermKeyCode.up, 125: TermKeyCode.down,
            124: TermKeyCode.right, 123: TermKeyCode.left,
            115: TermKeyCode.home, 119: TermKeyCode.end,
            116: TermKeyCode.pageUp, 121: TermKeyCode.pageDown,
            117: TermKeyCode.delete,
            122: TermKeyCode.f1, 120: TermKeyCode.f1 + 1,
            99: TermKeyCode.f1 + 2, 118: TermKeyCode.f1 + 3,
            96: TermKeyCode.f1 + 4, 97: TermKeyCode.f1 + 5,
            98: TermKeyCode.f1 + 6, 100: TermKeyCode.f1 + 7,
            101: TermKeyCode.f1 + 8, 109: TermKeyCode.f1 + 9,
            103: TermKeyCode.f1 + 10, 111: TermKeyCode.f1 + 11,
        ]

        private static func specialFor(_ keyCode: UInt16) -> Int32? {
            specialKeys[keyCode]
        }
    }

#else

    private struct TerminalKeyInput: UIViewRepresentable {
        var model: TerminalModel

        func makeUIView(context _: Context) -> TermKeyView {
            let view = TermKeyView()
            view.model = model
            return view
        }

        func updateUIView(_ view: TermKeyView, context _: Context) {
            view.model = model
        }
    }

    final class TermKeyView: UIView, UIKeyInput {
        weak var model: TerminalModel?

        override var canBecomeFirstResponder: Bool { true }

        override func didMoveToWindow() {
            super.didMoveToWindow()
            guard window != nil else { return }
            DispatchQueue.main.async { self.becomeFirstResponder() }
        }

        var hasText: Bool { true }

        var keyboardType: UIKeyboardType = .asciiCapable
        var autocorrectionType: UITextAutocorrectionType = .no
        var autocapitalizationType: UITextAutocapitalizationType = .none

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

    private struct TerminalExtraKeys: View {
        @Bindable var model: TerminalModel

        var body: some View {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    key("Esc") { model.typeSpecial(TermKeyCode.escape) }
                    key("Tab") { model.typeSpecial(TermKeyCode.tab) }
                    key("Ctrl", active: model.latchCtrl) { model.latchCtrl.toggle() }
                    key("Alt", active: model.latchAlt) { model.latchAlt.toggle() }
                    key("←") { model.typeSpecial(TermKeyCode.left) }
                    key("↓") { model.typeSpecial(TermKeyCode.down) }
                    key("↑") { model.typeSpecial(TermKeyCode.up) }
                    key("→") { model.typeSpecial(TermKeyCode.right) }
                    key("^C") {
                        model.sendKey(TermKeyCode.char, codepoint: 99, ctrl: true)
                        model.latchCtrl = false
                        model.latchAlt = false
                    }
                }
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
            }
        }

        private func key(
            _ label: String, active: Bool = false, action: @escaping () -> Void
        ) -> some View {
            Button(action: action) {
                Text(label)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(active ? DeskhubPalette.accent : Color.white)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 4)
                    .background(
                        RoundedCornerKeyBackground(active: active)
                    )
            }
            .buttonStyle(.plain)
        }
    }

    private struct RoundedCornerKeyBackground: View {
        let active: Bool

        var body: some View {
            RoundedRectangle(cornerRadius: 6)
                .fill(Color.white.opacity(active ? 0.25 : 0.12))
        }
    }

#endif
