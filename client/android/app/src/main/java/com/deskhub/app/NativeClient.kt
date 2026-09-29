package com.deskhub.app

import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.Surface
import android.view.WindowManager
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object NativeClient {
    const val PHASE_IDLE = 0
    const val PHASE_STREAMING = 2
    const val PHASE_ENDED = 3
    const val PHASE_REATTACHING = 5

    init {
        System.loadLibrary("deskhub")
    }

    @Suppress("DEPRECATION")
    fun screenSizePx(context: Context): Pair<Int, Int> {
        val wm =
            context.getSystemService(Context.WINDOW_SERVICE) as? WindowManager
                ?: return 0 to 0
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val b = wm.maximumWindowMetrics.bounds
            b.width() to b.height()
        } else {
            val p = android.graphics.Point()
            wm.defaultDisplay.getRealSize(p)
            p.x to p.y
        }
    }

    const val STR_CLIENT_IP_PROMPT = 3
    const val STR_QUERYING_SOURCES = 12
    const val STR_INVALID_ADDRESS_HINT = 17
    const val STR_SESSION_ENDED = 18
    const val STR_PROJECT_URL = 36
    const val STR_PROJECT_LINK_LABEL = 37
    const val STR_CLIENT_HEADING = 33
    const val STR_REQUEST_CONTROL_LABEL = 39
    const val STR_DEVICES_HEADING = 113
    const val STR_SIDEBAR_CLIENT = 30
    const val STR_SIDEBAR_SETTINGS = 31
    const val STR_SIDEBAR_HOST = 29
    const val STR_HOST_HEADING = 32
    const val STR_HOST_IP_INTRO = 1
    const val STR_NO_NETWORK_ADDRESS = 2
    const val STR_SHARING_TITLE = 7
    const val STR_SHARING_CONNECT_HINT = 9
    const val STR_NOTHING_SHARED = 10
    const val STR_STOP_SHARING = 11
    const val STR_SHARE_START_FAILED = 19
    const val STR_SHARE_STATE_ON = 47
    const val STR_SHARE_STATE_OFF = 48
    const val STR_RECEIVING_FILES_STATE = 168
    const val STR_MOBILE_TAKES_FILES_NOTE = 169
    const val STR_FILES_PICKER_LABEL = 133
    const val STR_START_SHARING = 49
    const val STR_STARTING_SHARE = 50
    const val STR_DISCONNECT_VIEWER_ACTION = 53
    const val STR_NOT_SHARING = 55
    const val STR_CLIENT_SETTINGS_HEADING = 57
    const val THEME_ACCENT = 4
    const val THEME_HEADING = 7
    const val THEME_MUTED = 8
    const val THEME_ONLINE = 10
    const val THEME_PAGE = 15
    const val STR_CLIENT_SETTINGS_HINT = 58
    const val STR_REFRESH_NOW = 51
    const val STR_UDP_PORT_LABEL = 59
    const val STR_BIND_INTERFACE_LABEL = 61
    const val STR_BIND_ALL_INTERFACES = 62
    const val STR_CLIPBOARD_SYNC_LABEL = 65
    const val STR_SHARE_AUDIO_LABEL = 130
    const val STR_PLAY_AUDIO_LABEL = 131
    const val STR_BIND_NOT_CONNECTED = 70
    const val STR_SECTION_CONNECTION = 72
    const val STR_SECTION_SESSION = 74
    const val STR_KEEP_AWAKE_LABEL = 76
    const val STR_SIDEBAR_DEVICES = 80
    const val STR_PAIRED_HEADING = 81
    const val STR_PAIRED_HINT = 82
    const val STR_PAIRED_EMPTY = 83
    const val STR_PAIRED_FORGET = 84
    const val STR_PAIRED_FORGET_ALL = 85
    const val STR_PAIRED_FORGET_ALL_PROMPT = 86
    const val STR_THIS_MACHINE_HEADING = 89
    const val STR_THIS_MACHINE_HINT = 90
    const val STR_OPEN_DESKTOP_LABEL = 105
    const val STR_OPEN_SHELL_LABEL = 106
    const val STR_CONNECT_BUTTON = 116
    const val STR_CONNECTED_PICK_SESSION = 150
    const val STR_TERMINAL_EXTRA_KEYS_HINT = 108

    const val STR_TRANSFER_CANCEL_BUTTON = 136
    const val STR_TRANSFER_SENDING = 138
    const val STR_TRANSFER_SEND_HEADING = 141
    const val STR_TRANSFER_NONE_CHOSEN = 142
    const val STR_TRANSFER_TOO_MANY_FILES = 144
    const val STR_OPEN_FILES_LABEL = 145
    const val STR_TRANSFER_SENT_HEADING = 146
    const val STR_TRANSFER_ARRIVED_TITLE = 148
    const val STR_DISCONNECT_BUTTON = 152
    const val STR_LINK_REATTACHING = 157
    const val STR_SHELL_PICKER_TITLE = 158
    const val STR_SHELL_PICKER_EMPTY = 159
    const val STR_SHELL_PICKER_NEW = 161
    const val STR_SHELL_PICKER_CLOSE = 162
    const val STR_SHELL_PICKER_CLOSE_ASK = 163
    const val STR_SAVED_HOSTS_HEADING = 171
    const val STR_SAVED_HOSTS_HINT = 172
    const val STR_SAVED_HOSTS_EMPTY = 173
    const val STR_REMOVE_HOST_ACTION = 180
    const val STR_PAIRED_FORGET_NOTE = 110
    const val STR_COPY_BUTTON = 117
    const val STR_DEVICES_HOST_AREA = 181
    const val STR_DEVICES_HOST_AREA_HINT = 182
    const val STR_DEVICES_CLIENT_AREA = 183
    const val STR_DEVICES_CLIENT_AREA_HINT = 184
    const val STR_ALLOW_CLIENT_PLACEHOLDER = 185
    const val STR_ALLOW_CLIENT_ACTION = 186
    const val STR_ALLOW_CLIENT_INVALID = 187
    const val STR_MY_KEYS_HEADING = 188
    const val STR_MY_KEYS_HINT = 189
    const val STR_COPY_PUBLIC_KEY_ACTION = 190
    const val STR_NEW_KEY_ACTION = 191
    const val STR_IMPORT_KEY_ACTION = 192
    const val STR_KEY_NAME_LABEL = 193
    const val STR_KEY_PASSPHRASE_LABEL = 194
    const val STR_TRUST_NEW_HOST_TITLE = 195
    const val STR_TRUST_NEW_HOST_ACTION = 196
    const val STR_CANCEL_ACTION = 197
    const val STR_COPIED_BUTTON = 198
    const val STR_DEVICE_NAME_LABEL = 115
    const val STR_DEVICE_NAME_HINT = 199
    const val STR_DELETE_KEY_ACTION = 200
    const val STR_DELETE_KEY_PROMPT = 201
    const val DEFAULT_KEY_NAME = "default"

    private const val HOST_PROFILE_OK = 0
    private const val HOST_PROFILE_STORE_UNREADABLE = 10
    private const val CLIENT_KEY_OK = 0
    private const val CLIENT_KEY_UNREADABLE = 3
    private const val FAILURE_SLOT = 0
    private const val NEW_HOST_KEY_SLOT = 1

    private external fun nativeString(id: Int): String

    private external fun nativeVersionLine(): String

    fun versionLine(): String = nativeVersionLine()

    private external fun nativeUdpPortLine(port: Int): String

    fun udpPortLine(port: Int): String = nativeUdpPortLine(port)

    private external fun nativeComposeAddress(
        host: String,
        portText: String,
    ): String

    fun composeAddress(
        host: String,
        portText: String,
    ): String = nativeComposeAddress(host, portText)

    private external fun nativeAddressHost(addr: String): String

    fun addressHost(addr: String): String = nativeAddressHost(addr)

    private external fun nativeAddressPort(addr: String): Int

    fun addressPort(addr: String): Int = nativeAddressPort(addr)

    private external fun nativeSetDataDir(dir: String)

    fun useAppDataDir(context: Context) {
        nativeSetDataDir(context.filesDir.absolutePath)
    }

    private external fun nativeParseAddress(addr: String): Boolean

    private external fun nativeCouldNotConnect(addr: String): String

    private external fun nativeConnectingTo(addr: String): String

    private external fun nativeSourceQueryFailed(addr: String): String

    private external fun nativeHostTitle(
        addr: String,
        width: Int,
        height: Int,
    ): String

    private external fun nativeZoomLabel(zoom: Float): String

    private external fun nativeIsZoomed(zoom: Float): Boolean

    fun string(id: Int): String = nativeString(id)

    private external fun nativeThemeColor(
        id: Int,
        dark: Boolean,
    ): Int

    fun themeColor(
        id: Int,
        dark: Boolean,
    ): Int = nativeThemeColor(id, dark)

    fun parseAddress(addr: String): Boolean = nativeParseAddress(addr)

    fun couldNotConnect(addr: String): String = nativeCouldNotConnect(addr)

    fun connectingTo(addr: String): String = nativeConnectingTo(addr)

    fun sourceQueryFailed(addr: String): String = nativeSourceQueryFailed(addr)

    fun hostTitle(
        addr: String,
        width: Int,
        height: Int,
    ): String = nativeHostTitle(addr, width, height)

    fun zoomLabel(zoom: Float): String = nativeZoomLabel(zoom)

    fun isZoomed(zoom: Float): Boolean = nativeIsZoomed(zoom)

    private external fun nativeListSources(
        addr: String,
        capsOut: BooleanArray,
        failureOut: Array<String>,
    ): Array<Source>?

    data class HostProfile(
        val alias: String,
        val endpoint: String,
        val identity: String,
        val fingerprint: String,
    )

    private external fun nativeHostProfiles(): Array<HostProfile>?

    private external fun nativeHostProfileRemove(alias: String): Int

    private external fun nativeHostProfileErrorText(error: Int): String

    private external fun nativeHostTrustNew(
        address: String,
        fingerprint: String,
    ): Int

    private external fun nativeTrustNewHostPrompt(
        address: String,
        fingerprint: String,
    ): String

    data class ClientKey(
        val name: String,
        val fingerprint: String,
    )

    private external fun nativeClientKeys(): Array<ClientKey>?

    private external fun nativeClientPublicKey(name: String): String

    private external fun nativeClientKeyGenerate(name: String): Int

    private external fun nativeClientKeyImport(
        name: String,
        privateKey: String,
        passphrase: String,
    ): Int

    private external fun nativeClientKeyDelete(name: String): Int

    private external fun nativeClientKeyErrorText(error: Int): String

    sealed interface HostProfiles {
        data class Loaded(
            val hosts: List<HostProfile>,
        ) : HostProfiles

        data class Unreadable(
            val message: String,
        ) : HostProfiles
    }

    private fun hostProfileFailure(error: Int): String? =
        if (error == HOST_PROFILE_OK) null else nativeHostProfileErrorText(error)

    fun hostProfiles(): HostProfiles =
        nativeHostProfiles()?.let { HostProfiles.Loaded(it.toList()) }
            ?: HostProfiles.Unreadable(nativeHostProfileErrorText(HOST_PROFILE_STORE_UNREADABLE))

    fun removeHostProfile(alias: String): String? = hostProfileFailure(nativeHostProfileRemove(alias))

    fun trustNewHost(
        address: String,
        fingerprint: String,
    ): String? = hostProfileFailure(nativeHostTrustNew(address, fingerprint))

    fun trustNewHostPrompt(
        address: String,
        fingerprint: String,
    ): String = nativeTrustNewHostPrompt(address, fingerprint)

    private fun clientKeyFailure(error: Int): String? =
        if (error ==
            CLIENT_KEY_OK
        ) {
            null
        } else {
            nativeClientKeyErrorText(error)
        }

    fun unreadableKeyText(): String = nativeClientKeyErrorText(CLIENT_KEY_UNREADABLE)

    fun clientKeys(): List<ClientKey> = nativeClientKeys()?.toList() ?: emptyList()

    fun clientPublicKey(name: String): String = nativeClientPublicKey(name)

    suspend fun generateClientKey(name: String): String? =
        withContext(Dispatchers.IO) { clientKeyFailure(nativeClientKeyGenerate(name)) }

    suspend fun importClientKey(
        name: String,
        privateKey: String,
        passphrase: String,
    ): String? = withContext(Dispatchers.IO) { clientKeyFailure(nativeClientKeyImport(name, privateKey, passphrase)) }

    suspend fun deleteClientKey(name: String): String? =
        withContext(Dispatchers.IO) { clientKeyFailure(nativeClientKeyDelete(name)) }

    data class DeviceRow(
        val addr: String,
        val name: String,
        val lastConnected: String,
    )

    data class PairedDevice(
        val name: String,
        val shortKey: String,
        val fingerprint: String,
    )

    private external fun nativePairedDevices(): Array<PairedDevice>?

    private external fun nativePairedForget(fingerprint: String): Boolean

    private external fun nativePairedForgetAll()

    private external fun nativePairedAddPublicKey(publicKey: String): Boolean

    private external fun nativeHostFingerprint(): String

    fun pairedDevices(): List<PairedDevice> = nativePairedDevices()?.toList() ?: emptyList()

    fun pairedForget(fingerprint: String): Boolean = nativePairedForget(fingerprint)

    fun pairedForgetAll() = nativePairedForgetAll()

    fun pairedAddPublicKey(publicKey: String): Boolean = nativePairedAddPublicKey(publicKey)

    fun hostFingerprint(): String = nativeHostFingerprint()

    private external fun nativeDefaultPort(): Int

    private external fun nativeClientControl(): Boolean

    private external fun nativeSetClientControl(on: Boolean)

    fun defaultPort(): Int = nativeDefaultPort()

    fun clientControl(): Boolean = nativeClientControl()

    fun setClientControl(on: Boolean) = nativeSetClientControl(on)

    private external fun nativeShareAudio(): Boolean

    private external fun nativeSetShareAudio(on: Boolean)

    private external fun nativePlayAudio(): Boolean

    private external fun nativeSetPlayAudio(on: Boolean)

    fun shareAudio(): Boolean = nativeShareAudio()

    fun setShareAudio(on: Boolean) = nativeSetShareAudio(on)

    fun playAudio(): Boolean = nativePlayAudio()

    fun setPlayAudio(on: Boolean) = nativeSetPlayAudio(on)

    private external fun nativeClipboardSync(): Boolean

    private external fun nativeSetClipboardSync(on: Boolean)

    fun clipboardSync(): Boolean = nativeClipboardSync()

    fun setClipboardSync(on: Boolean) = nativeSetClipboardSync(on)

    private external fun nativeKeepAwake(): Boolean

    private external fun nativeSetKeepAwake(on: Boolean)

    fun keepAwake(): Boolean = nativeKeepAwake()

    fun setKeepAwake(on: Boolean) = nativeSetKeepAwake(on)

    private external fun nativeMaxTransferFiles(): Int

    val maxTransferFiles: Int by lazy { nativeMaxTransferFiles() }

    private external fun nativeSendCheck(paths: Array<String>): String

    private external fun nativeSendStart(
        addr: String,
        name: String,
        paths: Array<String>,
    ): Long

    private external fun nativeSendSnapshot(handle: Long): Transfer?

    private external fun nativeSendChangedKey(handle: Long): String

    private external fun nativeSendAcceptKey(handle: Long): Boolean

    private external fun nativeSendCancel(handle: Long)

    private external fun nativeSendStop(handle: Long)

    fun sendCheck(paths: List<String>): String = nativeSendCheck(paths.toTypedArray())

    fun sendStart(
        addr: String,
        name: String,
        paths: List<String>,
    ): Long = nativeSendStart(addr, name, paths.toTypedArray())

    fun sendSnapshot(handle: Long): Transfer = nativeSendSnapshot(handle) ?: Transfer()

    fun sendChangedKey(handle: Long): String = nativeSendChangedKey(handle)

    fun sendAcceptKey(handle: Long): Boolean = nativeSendAcceptKey(handle)

    fun sendCancel(handle: Long) = nativeSendCancel(handle)

    fun sendStop(handle: Long) = nativeSendStop(handle)

    private external fun nativeClipOffer(text: String)

    private external fun nativeClipTake(): String

    fun clipOffer(text: String) = nativeClipOffer(text)

    fun clipTake(): String = nativeClipTake()

    private external fun nativeDeviceName(): String

    private external fun nativeSetDeviceName(name: String)

    fun deviceName(): String = nativeDeviceName()

    fun setDeviceName(name: String) = nativeSetDeviceName(name)

    fun sessionDeviceName(): String = deviceName().trim().ifBlank { Build.MODEL.orEmpty() }

    private external fun nativeSettingsPort(): Int

    private external fun nativeSetSettingsPort(port: Int)

    fun settingsPort(): Int = nativeSettingsPort()

    fun setSettingsPort(port: Int) = nativeSetSettingsPort(port)

    private external fun nativeDeviceRows(): Array<DeviceRow>

    suspend fun deviceRows(): List<DeviceRow> = withContext(Dispatchers.IO) { nativeDeviceRows().toList() }

    external fun nativeStart(
        addr: String,
        sourceId: Int,
        screenW: Int,
        screenH: Int,
    ): Long

    external fun nativeStop(handle: Long)

    interface SessionListener {
        fun onStatus(
            line: String,
            phase: Int,
        )

        fun onSize(
            width: Int,
            height: Int,
        )

        fun onEnded(reason: String)

        fun onTrustAsked(
            verdict: Int,
            fingerprint: String,
        ) {}
    }

    @Volatile
    var sessionListener: SessionListener? = null

    private val mainHandler = Handler(Looper.getMainLooper())

    @JvmStatic
    @JvmName("onSessionStatus")
    internal fun onSessionStatus(
        line: String,
        phase: Int,
    ) {
        mainHandler.post { sessionListener?.onStatus(line, phase) }
    }

    @JvmStatic
    @JvmName("onSessionSize")
    internal fun onSessionSize(
        width: Int,
        height: Int,
    ) {
        mainHandler.post { sessionListener?.onSize(width, height) }
    }

    @JvmStatic
    @JvmName("onSessionEnded")
    internal fun onSessionEnded(reason: String) {
        mainHandler.post { sessionListener?.onEnded(reason) }
    }

    @JvmStatic
    @JvmName("onSessionTrustAsked")
    internal fun onSessionTrustAsked(
        verdict: Int,
        fingerprint: String,
    ) {
        mainHandler.post { sessionListener?.onTrustAsked(verdict, fingerprint) }
    }

    private external fun nativeAcceptKey()

    private external fun nativeRejectKey()

    fun acceptKey() = nativeAcceptKey()

    fun rejectKey() = nativeRejectKey()

    external fun nativeSetSurface(surface: Surface?)

    external fun nativeReleaseSurface(surface: Surface)

    const val MOUSE_LEFT = 1
    const val MOUSE_RIGHT = 2

    private external fun nativeKey(
        vk: Int,
        scan: Int,
        down: Boolean,
    )

    private external fun nativeVkScancode(vk: Int): Int

    private external fun nativeKeyToVk(keyCode: Int): Int

    private external fun nativeConnectDecision(sourceIds: IntArray): Int

    private external fun nativeMouseMove(
        nx: Int,
        ny: Int,
    )

    private external fun nativeMouseButton(
        button: Int,
        down: Boolean,
    )

    private external fun nativeHotkey(
        vk: Int,
        scan: Int,
        modVk: Int,
        modScan: Int,
    )

    private external fun nativeMouseWheel(notches: Int)

    private external fun nativeCharTap(codepoint: Int)

    private external fun nativeReleaseAllInput()

    fun key(
        vk: Int,
        scan: Int,
        down: Boolean,
    ) {
        nativeKey(vk, scan, down)
    }

    fun vkScancode(vk: Int): Int = nativeVkScancode(vk)

    fun keyToVk(keyCode: Int): Int = nativeKeyToVk(keyCode)

    fun connectDecision(sources: List<Source>): Int = nativeConnectDecision(sources.map { it.id }.toIntArray())

    fun mouseMove(
        nx: Int,
        ny: Int,
    ) {
        nativeMouseMove(nx, ny)
    }

    fun mouseButton(
        button: Int,
        down: Boolean,
    ) {
        nativeMouseButton(button, down)
    }

    fun hotkey(hotkey: Hotkey) {
        nativeHotkey(hotkey.vk, hotkey.scan, hotkey.modVk, hotkey.modScan)
    }

    fun mouseWheel(notches: Int) {
        nativeMouseWheel(notches)
    }

    fun charTap(codepoint: Int) {
        nativeCharTap(codepoint)
    }

    fun releaseAllInput() {
        nativeReleaseAllInput()
    }

    external fun nativeVideoFrame(
        viewportW: Float,
        viewportH: Float,
        aspect: Float,
        zoom: Float,
        panX: Float,
        panY: Float,
    ): FloatArray

    private external fun nativeTakeScrollNotches(
        dragPoints: Float,
        carry: DoubleArray,
    ): Int

    fun takeScrollNotches(
        dragPoints: Float,
        carry: DoubleArray,
    ): Int = nativeTakeScrollNotches(dragPoints, carry)

    private external fun nativeCursorClamp(
        cx: Float,
        cy: Float,
        rectX: Float,
        rectY: Float,
        rectW: Float,
        rectH: Float,
        viewportW: Float,
        viewportH: Float,
    ): FloatArray

    private external fun nativeCursorMove(
        cx: Float,
        cy: Float,
        dx: Float,
        dy: Float,
        rectX: Float,
        rectY: Float,
        rectW: Float,
        rectH: Float,
        viewportW: Float,
        viewportH: Float,
    ): FloatArray

    private external fun nativeCursorPoint(
        cx: Float,
        cy: Float,
        rectX: Float,
        rectY: Float,
        rectW: Float,
        rectH: Float,
    ): FloatArray

    private external fun nativeCursorNormalize(
        cx: Float,
        cy: Float,
        rectX: Float,
        rectY: Float,
        rectW: Float,
        rectH: Float,
    ): IntArray

    data class Cursor(
        val x: Float = 0.5f,
        val y: Float = 0.5f,
    )

    fun cursorClamped(
        cursor: Cursor,
        video: Rect,
        viewport: Size,
    ): Cursor =
        nativeCursorClamp(
            cursor.x,
            cursor.y,
            video.left,
            video.top,
            video.width,
            video.height,
            viewport.width,
            viewport.height,
        ).let { Cursor(it[0], it[1]) }

    fun cursorMoved(
        cursor: Cursor,
        delta: Offset,
        video: Rect,
        viewport: Size,
    ): Cursor =
        nativeCursorMove(
            cursor.x,
            cursor.y,
            delta.x,
            delta.y,
            video.left,
            video.top,
            video.width,
            video.height,
            viewport.width,
            viewport.height,
        ).let { Cursor(it[0], it[1]) }

    fun cursorScreenPoint(
        cursor: Cursor,
        video: Rect,
    ): Offset? =
        nativeCursorPoint(
            cursor.x,
            cursor.y,
            video.left,
            video.top,
            video.width,
            video.height,
        ).takeIf { it.size == 2 }?.let { Offset(it[0], it[1]) }

    fun cursorMouseMove(
        cursor: Cursor,
        video: Rect,
    ) {
        val n =
            nativeCursorNormalize(
                cursor.x,
                cursor.y,
                video.left,
                video.top,
                video.width,
                video.height,
            )
        if (n.size == 2) mouseMove(n[0], n[1])
    }

    private external fun nativeApplyGesture(
        zoom: Float,
        panX: Float,
        panY: Float,
        factor: Float,
        centroidX: Float,
        centroidY: Float,
        panDeltaX: Float,
        panDeltaY: Float,
        viewportW: Float,
        viewportH: Float,
        aspect: Float,
    ): FloatArray

    data class Transform(
        val zoom: Float,
        val panX: Float,
        val panY: Float,
    )

    fun applyGesture(
        current: Transform,
        factor: Float,
        centroidX: Float,
        centroidY: Float,
        panDeltaX: Float,
        panDeltaY: Float,
        viewportW: Float,
        viewportH: Float,
        aspect: Float,
    ): Transform {
        val r =
            nativeApplyGesture(
                current.zoom,
                current.panX,
                current.panY,
                factor,
                centroidX,
                centroidY,
                panDeltaX,
                panDeltaY,
                viewportW,
                viewportH,
                aspect,
            )
        return Transform(r[0], r[1], r[2])
    }

    private external fun nativeHotkeys(): Array<Hotkey>

    data class Hotkey(
        val label: String,
        val vk: Int,
        val scan: Int,
        val modVk: Int,
        val modScan: Int,
    )

    val hotkeys: List<Hotkey> by lazy { nativeHotkeys().toList() }

    data class Source(
        val id: Int,
        val displayName: String,
        val sizeLabel: String,
    )

    data class Transfer(
        val active: Boolean = false,
        val done: Boolean = false,
        val failed: Boolean = false,
        val fileIndex: Int = 0,
        val fileCount: Int = 0,
        val bytes: Long = 0,
        val total: Long = 0,
        val name: String = "",
        val message: String = "",
    ) {
        val idle: Boolean get() = !active && !done && !failed

        val fraction: Float
            get() {
                if (total <= 0) return if (active) 0f else 1f
                return minOf(1f, bytes.toFloat() / total.toFloat())
            }

        val step: String
            get() {
                if (fileCount <= 0) return ""
                return "${minOf(fileIndex + 1, fileCount)}/$fileCount"
            }
    }

    data class Snapshot(
        val phase: Int,
        val statusLine: String,
        val endReason: String,
        val videoWidth: Int,
        val videoHeight: Int,
    )

    external fun nativeSnapshot(): Snapshot?

    data class HostQuery(
        val sources: List<Source>,
        val terminal: Boolean,
        val files: Boolean,
    )

    sealed interface QueryOutcome {
        data class Reached(
            val query: HostQuery,
        ) : QueryOutcome

        data class UnknownHost(
            val fingerprint: String,
            val reason: String,
        ) : QueryOutcome

        data class Failed(
            val reason: String,
        ) : QueryOutcome
    }

    suspend fun queryHost(addr: String): QueryOutcome =
        withContext(Dispatchers.IO) {
            val caps = BooleanArray(2)
            val failure = arrayOf("", "")
            val sources = nativeListSources(addr, caps, failure)
            if (sources != null) return@withContext QueryOutcome.Reached(HostQuery(sources.toList(), caps[0], caps[1]))
            val reason = failure[FAILURE_SLOT].ifBlank { sourceQueryFailed(addr) }
            val newHostKey = failure[NEW_HOST_KEY_SLOT]
            if (newHostKey.isBlank()) QueryOutcome.Failed(reason) else QueryOutcome.UnknownHost(newHostKey, reason)
        }
}
