import ReplayKit
import SwiftUI

@MainActor
final class BroadcastPickerHandle {
    fileprivate weak var picker: RPSystemBroadcastPickerView?

    func present() {
        guard let picker, let button = BroadcastPickerHandle.firstButton(in: picker) else {
            return
        }
        button.sendActions(for: .touchUpInside)
    }

    private static func firstButton(in view: UIView) -> UIButton? {
        if let button = view as? UIButton { return button }
        for subview in view.subviews {
            if let button = firstButton(in: subview) { return button }
        }
        return nil
    }
}

struct BroadcastPickerBridge: UIViewRepresentable {
    static let offscreenSize = CGSize(width: 1, height: 1)
    static let offscreenOpacity = 0.01

    let extensionBundleId: String
    let handle: BroadcastPickerHandle

    func makeUIView(context _: Context) -> RPSystemBroadcastPickerView {
        let picker = RPSystemBroadcastPickerView(
            frame: CGRect(origin: .zero, size: BroadcastPickerBridge.offscreenSize)
        )
        picker.preferredExtension = extensionBundleId
        picker.showsMicrophoneButton = false
        handle.picker = picker
        return picker
    }

    func updateUIView(_ picker: RPSystemBroadcastPickerView, context _: Context) {
        picker.preferredExtension = extensionBundleId
        handle.picker = picker
    }
}

struct BroadcastPickerButton: View {
    let extensionBundleId: String
    let title: String

    @State private var handle = BroadcastPickerHandle()

    var body: some View {
        Button(title) { handle.present() }
            .buttonStyle(.iosFilled(fullWidth: true))
            .background(
                BroadcastPickerBridge(extensionBundleId: extensionBundleId, handle: handle)
                    .frame(
                        width: BroadcastPickerBridge.offscreenSize.width,
                        height: BroadcastPickerBridge.offscreenSize.height
                    )
                    .opacity(BroadcastPickerBridge.offscreenOpacity)
                    .allowsHitTesting(false)
            )
    }
}
