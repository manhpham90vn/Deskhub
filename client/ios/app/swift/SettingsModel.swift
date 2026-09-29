import Foundation
import Observation

@MainActor @Observable
final class SettingsModel {
    var port: Int
    var clientControl: Bool
    var clipboardSync: Bool
    var shareAudio: Bool
    var playAudio: Bool
    var keepAwake: Bool

    init() {
        let stored = dh_settings_load()
        port = Int(stored.port)
        clientControl = stored.clientControl
        clipboardSync = dh_clipboard_sync()
        shareAudio = dh_share_audio()
        playAudio = dh_play_audio()
        keepAwake = dh_keep_awake()
    }

    var acceptedPort: UInt16 {
        port >= 1 && port <= 65535 ? UInt16(port) : DeskhubDiscovery.defaultPort
    }

    func save() {
        let stored = dh_settings_load()
        dh_settings_save(
            stored.fps, stored.bitrateMbps, stored.maxDim, UInt32(acceptedPort),
            stored.allowInput, clientControl
        )
        dh_set_clipboard_sync(clipboardSync)
        dh_set_share_audio(shareAudio)
        dh_set_play_audio(playAudio)
        dh_set_keep_awake(keepAwake)
    }
}
