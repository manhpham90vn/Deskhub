package com.deskhub.app

import android.content.Context
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.projection.MediaProjection
import android.os.Build
import android.view.Surface
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object NativeHost {
    private const val VIRTUAL_DISPLAY_NAME = "Deskhub"
    private const val DEFAULT_DENSITY_DPI = 320

    init {
        System.loadLibrary("deskhub")
    }

    val isSupported: Boolean
        get() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q

    enum class ShareState {
        IDLE,
        STARTING,
        SHARING,
        FAILED,
    }

    @Volatile
    var shareState = ShareState.IDLE
        private set

    @Volatile
    var shareError = ""
        private set

    private external fun nativeSetScreenSize(
        width: Int,
        height: Int,
        name: String,
    )

    private external fun nativeStart(
        fps: Int,
        bitrateMbps: Int,
        maxDim: Int,
        port: Int,
        passcode: String,
        transferDir: String,
    ): Boolean

    private external fun nativeOfferAudio(
        pcm: ShortArray,
        samples: Int,
    )

    private external fun nativeAudioRunning(): Boolean

    fun offerAudio(
        pcm: ShortArray,
        samples: Int,
    ) = nativeOfferAudio(pcm, samples)

    fun audioRunning(): Boolean = nativeAudioRunning()

    private external fun nativeStop()

    private external fun nativeRunning(): Boolean

    private external fun nativeLastError(): String

    private external fun nativeLocalAddresses(): String

    private external fun nativeKickViewer(
        sourceId: Int,
        viewerAddr: String,
    )

    private external fun nativeProjectionStopped()

    private external fun nativeDisplayResized(
        width: Int,
        height: Int,
    )

    private external fun nativeHostRows(): Array<HostRow>

    private external fun nativeSharingStatus(
        port: Int,
        passcode: String,
        screen: Boolean,
    ): String

    private external fun nativeIdleStatus(port: Int): String

    private external fun nativePasscode(): String

    private external fun nativePasscodeDisplay(passcode: String): String

    private external fun nativeSavePasscode(passcode: String)

    private external fun nativeShareDefaults(): IntArray

    private external fun nativeClipOffer(text: String)

    private external fun nativeClipTake(): String

    fun clipOffer(text: String) = nativeClipOffer(text)

    fun clipTake(): String = nativeClipTake()

    private external fun nativeBindIp(): String

    private external fun nativeSetBindIp(ip: String)

    fun bindIp(): String = nativeBindIp()

    fun setBindIp(ip: String) = nativeSetBindIp(ip)

    private external fun nativeTakePairingRequests(): Array<PairingRequest>?

    private external fun nativeAnswerPairing(
        addrPacked: Long,
        allow: Boolean,
    )

    fun takePairingRequests(): List<PairingRequest> = nativeTakePairingRequests()?.toList() ?: emptyList()

    fun answerPairing(
        addrPacked: Long,
        allow: Boolean,
    ) = nativeAnswerPairing(addrPacked, allow)

    data class HostRow(
        val viewer: Boolean,
        val sourceId: Int,
        val online: Boolean,
        val viewerAddr: String,
        val source: String,
        val size: String,
        val viewers: String,
        val client: String,
        val capture: String,
        val send: String,
        val mbps: String,
        val rtt: String,
    )

    data class LocalAddress(
        val ip: String,
        val name: String,
    )

    data class PairingRequest(
        val addrPacked: Long,
        val shortKey: String,
        val body: String,
    )

    data class ShareDefaults(
        val fps: Int,
        val bitrateMbps: Int,
        val maxDim: Int,
    )

    fun publishScreenSize(context: Context) {
        val (width, height) = NativeClient.screenSizePx(context)
        if (width <= 0 || height <= 0) return
        nativeSetScreenSize(width, height, Build.MODEL.ifBlank { VIRTUAL_DISPLAY_NAME })
    }

    fun displayResized(context: Context) {
        val (width, height) = NativeClient.screenSizePx(context)
        if (width <= 0 || height <= 0) return
        nativeDisplayResized(width, height)
    }

    fun awaitStart() {
        shareError = ""
        shareState = ShareState.STARTING
    }

    fun reportFailure(message: String) {
        shareError = message.ifBlank { NativeClient.string(NativeClient.STR_SHARE_START_FAILED) }
        shareState = ShareState.FAILED
    }

    private external fun nativeStartFilesOnly(transferDir: String): Boolean

    private external fun nativeStopFilesOnly()

    private external fun nativeFilesActive(): Boolean

    fun filesActive(): Boolean = nativeFilesActive()

    suspend fun start(
        request: HostService.ShareRequest,
        transferDir: String,
    ): Boolean =
        withContext(Dispatchers.IO) {
            val ok =
                nativeStart(
                    request.fps,
                    request.bitrateMbps,
                    request.maxDim,
                    request.port,
                    request.passcode,
                    transferDir,
                )
            if (ok) shareState = ShareState.SHARING
            ok
        }

    suspend fun startFiles(transferDir: String): Boolean =
        withContext(Dispatchers.IO) { nativeStartFilesOnly(transferDir) }

    suspend fun stopFiles() = withContext(Dispatchers.IO) { nativeStopFilesOnly() }

    fun stop() {
        nativeStop()
        if (shareState != ShareState.FAILED) {
            shareState = ShareState.IDLE
            shareError = ""
        }
    }

    fun isRunning(): Boolean = nativeRunning()

    fun lastError(): String = nativeLastError()

    fun hostRows(): List<HostRow> = nativeHostRows().toList()

    fun kickViewer(
        sourceId: Int,
        viewerAddr: String,
    ) = nativeKickViewer(sourceId, viewerAddr)

    fun sharingStatus(
        port: Int,
        passcode: String,
        screen: Boolean,
    ): String = nativeSharingStatus(port, passcode, screen)

    fun idleStatus(port: Int): String = nativeIdleStatus(port)

    fun passcode(): String = nativePasscode()

    fun passcodeDisplay(passcode: String): String = nativePasscodeDisplay(passcode)

    fun savePasscode(passcode: String) = nativeSavePasscode(passcode)

    fun shareDefaults(): ShareDefaults = nativeShareDefaults().let { ShareDefaults(it[0], it[1], it[2]) }

    fun localAddresses(): List<LocalAddress> =
        nativeLocalAddresses()
            .split("\n")
            .filter { it.isNotBlank() }
            .map { line ->
                val parts = line.split("\t", limit = 2)
                LocalAddress(parts[0], parts.getOrElse(1) { "" })
            }

    @Volatile
    private var projection: MediaProjection? = null

    @Volatile
    private var virtualDisplay: VirtualDisplay? = null

    @Volatile
    private var densityDpi = 0

    fun useProjection(
        context: Context,
        granted: MediaProjection,
    ) {
        densityDpi = context.resources.displayMetrics.densityDpi
        projection = granted
    }

    fun releaseProjection() {
        detachSurface()
        projection?.stop()
        projection = null
    }

    fun onProjectionStopped() {
        detachSurface()
        projection = null
        nativeProjectionStopped()
    }

    @JvmStatic
    @JvmName("projectionReady")
    internal fun projectionReady(): Boolean = projection != null

    @JvmStatic
    @JvmName("attachSurface")
    internal fun attachSurface(
        surface: Surface,
        width: Int,
        height: Int,
    ): Boolean {
        val active = projection ?: return false
        detachSurface()
        virtualDisplay =
            runCatching {
                active.createVirtualDisplay(
                    VIRTUAL_DISPLAY_NAME,
                    width,
                    height,
                    if (densityDpi > 0) densityDpi else DEFAULT_DENSITY_DPI,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                    surface,
                    null,
                    null,
                )
            }.getOrNull()
        return virtualDisplay != null
    }

    @JvmStatic
    @JvmName("detachSurface")
    internal fun detachSurface() {
        virtualDisplay?.release()
        virtualDisplay = null
    }
}
