import SwiftUI

struct ConnectionStatusBar: View {
    let model: StreamModel
    let onToggleCursor: () -> Void
    let onFullScreen: () -> Void

    var body: some View {
        HStack(spacing: 10) {
            if model.reattaching {
                ProgressView()
                    .controlSize(.small)
                Text(DeskhubClient.string(DHStrLinkReattaching))
            }
            Spacer(minLength: 12)
            if model.control {
                Button(
                    DeskhubClient.cursorButtonLabel(hidden: model.cursorHidden),
                    action: onToggleCursor
                )
            }
            Button(DeskhubClient.string(DHStrFullScreenButton), action: onFullScreen)
        }
        .font(.callout)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(.bar)
    }
}
