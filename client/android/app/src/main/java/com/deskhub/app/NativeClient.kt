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
    const val STR_CLIENT_PASSCODE_PROMPT = 21
    const val STR_CLIENT_PASSCODE_HINT = 22
    const val STR_PASSCODE_INVALID = 23
    const val STR_CONNECT_PROMPT_TITLE = 41
    const val STR_PROJECT_URL = 36
    const val STR_PROJECT_LINK_LABEL = 37
    const val STR_CLIENT_HEADING = 33
    const val STR_REQUEST_CONTROL_LABEL = 39
    const val STR_LAN_DEVICES_HEADING = 25
    const val STR_RECENT_DEVICES_HEADING = 26
    const val STR_DEVICES_HEADING = 113
    const val STR_RECENT_DEVICES_HINT = 27
    const val STR_RECENT_DEVICES_EMPTY = 28
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
    const val STR_PASSCODE_LABEL = 24
    const val STR_SHARE_STATE_ON = 47
    const val STR_SHARE_STATE_OFF = 48
    const val STR_START_SHARING = 49
    const val STR_STARTING_SHARE = 50
    const val STR_DISCONNECT_VIEWER_ACTION = 53
    const val STR_NOT_SHARING = 55
    const val STR_CLIENT_SETTINGS_HEADING = 57
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
    const val STR_PAIRING_REQUEST_TITLE = 77
    const val STR_PAIRING_ALLOW = 78
    const val STR_PAIRING_DENY = 79
    const val STR_SIDEBAR_DEVICES = 80
    const val STR_PAIRED_HEADING = 81
    const val STR_PAIRED_HINT = 82
    const val STR_PAIRED_EMPTY = 83
    const val STR_PAIRED_FORGET = 84
    const val STR_PAIRED_FORGET_ALL = 85
    const val STR_PAIRED_FORGET_ALL_PROMPT = 86
    const val STR_ALLOW_PAIRING_LABEL = 87
    const val STR_ALLOW_PAIRING_HINT = 88
    const val STR_THIS_MACHINE_HEADING = 89
    const val STR_THIS_MACHINE_HINT = 90
    const val STR_TRUST_NEW_HOST_TITLE = 95
    const val STR_TRUST_NEW_HOST_BODY = 96
    const val STR_TRUST_CHANGED_TITLE = 97
    const val STR_TRUST_CHANGED_BODY = 98
    const val STR_TRUST_FINGERPRINT_LABEL = 99
    const val STR_TRUST_ACCEPT = 100
    const val STR_TRUST_REJECT = 101
    const val STR_OPEN_DESKTOP_LABEL = 105
    const val STR_OPEN_SHELL_LABEL = 106
    const val STR_CONNECT_BUTTON = 116
    const val STR_CONNECTED_PICK_SESSION = 151
    const val STR_TERMINAL_EXTRA_KEYS_HINT = 108

    const val TRUST_CHANGED = 2

    const val STR_TRANSFER_CHOOSE_BUTTON = 135
    const val STR_TRANSFER_CANCEL_BUTTON = 136
    const val STR_TRANSFER_SENDING = 138
    const val STR_TRANSFER_SEND_HEADING = 141
    const val STR_TRANSFER_NONE_CHOSEN = 142
    const val STR_TRANSFER_TOO_MANY_FILES = 144
    const val STR_OPEN_FILES_LABEL = 145
    const val STR_TRANSFER_SENT_HEADING = 146
    const val STR_TRANSFER_ARRIVED_TITLE = 149
    const val STR_DISCONNECT_BUTTON = 153
    const val STR_LINK_REATTACHING = 158

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

    private external fun nativeSameDeviceAddr(
        left: String,
        right: String,
    ): Boolean

    fun sameDeviceAddr(
        left: String,
        right: String,
    ): Boolean = nativeSameDeviceAddr(left, right)

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
        passcode: String,
        capsOut: BooleanArray,
    ): Array<Source>?

    private external fun nativeIsValidPasscode(passcode: String): Boolean

    private external fun nativePasscodeDigits(): Int

    fun isValidPasscode(passcode: String): Boolean = nativeIsValidPasscode(passcode)

    fun passcodeDigits(): Int = nativePasscodeDigits()

    data class DeviceRow(
        val addr: String,
        val passcode: String,
        val origin: String,
        val status: String,
        val ping: String,
        val lastConnected: String,
        val known: Boolean,
        val online: Boolean,
    )

    data class PairedDevice(
        val name: String,
        val shortKey: String,
        val fingerprint: String,
        val pairedUnix: Long,
        val lastSeenUnix: Long,
    )

    private external fun nativePairedDevices(): Array<PairedDevice>?

    private external fun nativePairedForget(fingerprint: String): Boolean

    private external fun nativePairedForgetAll()

    private external fun nativeAllowPairing(): Boolean

    private external fun nativeSetAllowPairing(allow: Boolean)

    private external fun nativeOwnFingerprint(): String

    fun pairedDevices(): List<PairedDevice> = nativePairedDevices()?.toList() ?: emptyList()

    fun pairedForget(fingerprint: String): Boolean = nativePairedForget(fingerprint)

    fun pairedForgetAll() = nativePairedForgetAll()

    fun allowPairing(): Boolean = nativeAllowPairing()

    fun setAllowPairing(allow: Boolean) = nativeSetAllowPairing(allow)

    fun ownFingerprint(): String = nativeOwnFingerprint()

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
        passcode: String,
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
        passcode: String,
        name: String,
        paths: List<String>,
    ): Long = nativeSendStart(addr, passcode, name, paths.toTypedArray())

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

    private external fun nativeSettingsPort(): Int

    private external fun nativeSetSettingsPort(port: Int)

    fun settingsPort(): Int = nativeSettingsPort()

    fun setSettingsPort(port: Int) = nativeSetSettingsPort(port)

    private external fun nativeScanStart(port: Int): Boolean

    private external fun nativeScanRestart(port: Int): Boolean

    private external fun nativeRescanSeconds(): Int

    private external fun nativeStatusRefreshNow()

    private external fun nativeScanCancel()

    private external fun nativeScanRunning(): Boolean

    private external fun nativeScanStatusText(port: Int): String

    private external fun nativeDeviceRows(): Array<DeviceRow>

    private external fun nativeRecentTouch(
        addr: String,
        passcode: String,
    )

    private external fun nativeRecentPasscode(addr: String): String

    private external fun nativeWatchRecent()

    fun scanStart(port: Int): Boolean = nativeScanStart(port)

    suspend fun scanRestart(port: Int): Boolean = withContext(Dispatchers.IO) { nativeScanRestart(port) }

    fun rescanSeconds(): Int = nativeRescanSeconds()

    suspend fun statusRefreshNow() = withContext(Dispatchers.IO) { nativeStatusRefreshNow() }

    fun scanCancel() = nativeScanCancel()

    fun scanRunning(): Boolean = nativeScanRunning()

    fun scanStatusText(port: Int): String = nativeScanStatusText(port)

    suspend fun deviceRows(): List<DeviceRow> = withContext(Dispatchers.IO) { nativeDeviceRows().toList() }

    suspend fun recentTouch(
        addr: String,
        passcode: String,
    ) = withContext(Dispatchers.IO) { nativeRecentTouch(addr, passcode) }

    fun recentPasscode(addr: String): String = nativeRecentPasscode(addr)

    suspend fun watchRecent() = withContext(Dispatchers.IO) { nativeWatchRecent() }

    external fun nativeStart(
        addr: String,
        sourceId: Int,
        screenW: Int,
        screenH: Int,
        passcode: String,
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
        )
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

    suspend fun queryHost(
        addr: String,
        passcode: String,
    ): HostQuery? =
        withContext(Dispatchers.IO) {
            val caps = BooleanArray(2)
            val sources = nativeListSources(addr, passcode, caps) ?: return@withContext null
            HostQuery(sources.toList(), caps[0], caps[1])
        }
}
