import SwiftUI
import UIKit

struct KeyInputView: UIViewRepresentable {
    let model: StreamModel
    @Binding var active: Bool

    func makeUIView(context _: Context) -> KeyCaptureUIView {
        let view = KeyCaptureUIView()
        view.model = model
        return view
    }

    func updateUIView(_ uiView: KeyCaptureUIView, context _: Context) {
        uiView.model = model
        let binding = $active
        uiView.onKeyboardDismissed = { binding.wrappedValue = false }
        if active, !uiView.isFirstResponder {
            uiView.becomeFirstResponder()
        } else if !active, uiView.isFirstResponder {
            uiView.resignFirstResponder()
        }
    }
}

final class KeyCaptureUIView: UIView, UIKeyInput {
    weak var model: StreamModel?
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
        for scalar in text.unicodeScalars {
            model?.charTap(scalar.value)
        }
    }

    func deleteBackward() {
        model?.charTap(0x08)
    }

    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let unhandled = handlePresses(presses, down: true)
        if !unhandled.isEmpty { super.pressesBegan(unhandled, with: event) }
    }

    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let unhandled = handlePresses(presses, down: false)
        if !unhandled.isEmpty { super.pressesEnded(unhandled, with: event) }
    }

    override func pressesCancelled(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let unhandled = handlePresses(presses, down: false)
        if !unhandled.isEmpty { super.pressesCancelled(unhandled, with: event) }
    }

    private func handlePresses(_ presses: Set<UIPress>, down: Bool) -> Set<UIPress> {
        var unhandled = Set<UIPress>()
        for press in presses {
            guard let key = press.key else {
                unhandled.insert(press)
                continue
            }
            if let mapped = DeskhubClient.mapKey(Int32(key.keyCode.rawValue)) {
                model?.key(vk: mapped.vk, scan: mapped.scan, down: down)
                continue
            }
            if down, let scalar = chordScalar(for: key) {
                model?.charTap(scalar.value)
                continue
            }
            unhandled.insert(press)
        }
        return unhandled
    }

    private func chordScalar(for key: UIKey) -> Unicode.Scalar? {
        let held: UIKeyModifierFlags = [.control, .command, .alternate]
        guard !key.modifierFlags.isDisjoint(with: held) else { return nil }
        guard let scalar = key.charactersIgnoringModifiers.unicodeScalars.first,
              scalar.value >= 0x20 else { return nil }
        return scalar
    }
}
