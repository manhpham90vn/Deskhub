package com.deskhub.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.content.res.Configuration
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

private const val TAG = "Deskhub"

class HostService : Service() {
    private var projection: MediaProjection? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private val scope = CoroutineScope(SupervisorJob())
    private var clipboardJob: Job? = null

    private val projectionCallback =
        object : MediaProjection.Callback() {
            override fun onStop() {
                NativeHost.onProjectionStopped()
                mainHandler.post { stopEverything() }
            }
        }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int,
    ): Int {
        if (intent == null || intent.action == ACTION_STOP) {
            stopEverything()
            return START_NOT_STICKY
        }

        val resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, 0)
        val consent = readConsent(intent)
        if (resultCode == 0 || consent == null) {
            failWith(NativeClient.string(NativeClient.STR_SHARE_START_FAILED))
            return START_NOT_STICKY
        }

        ServiceCompat.startForeground(this, NOTIFICATION_ID, buildNotification(), foregroundType())

        val manager = getSystemService(MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
        val granted = manager.getMediaProjection(resultCode, consent)
        if (granted == null) {
            failWith(NativeClient.string(NativeClient.STR_SHARE_START_FAILED))
            return START_NOT_STICKY
        }

        projection = granted
        granted.registerCallback(projectionCallback, mainHandler)
        NativeHost.useProjection(this, granted)
        NativeHost.publishScreenSize(this)

        val options = ShareRequest.from(intent)
        val transferDir = ReceivedFiles.transferDir(applicationContext)
        scope.launch {
            val ok = NativeHost.start(options, transferDir)
            if (!ok) {
                mainHandler.post { failWith(NativeHost.lastError()) }
            } else if (NativeHost.audioRunning()) {
                AudioShare.start(applicationContext, granted)
            }
        }
        clipboardJob?.cancel()
        if (NativeClient.clipboardSync()) {
            clipboardJob =
                scope.launch(Dispatchers.Main) {
                    ClipboardPump.run(
                        applicationContext,
                        take = { NativeHost.clipTake() },
                        offer = { NativeHost.clipOffer(it) },
                    )
                }
        }
        return START_NOT_STICKY
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        NativeHost.displayResized(this)
    }

    override fun onDestroy() {
        scope.cancel()
        super.onDestroy()
    }

    private fun failWith(message: String) {
        NativeHost.reportFailure(message)
        stopEverything()
    }

    private fun stopEverything() {
        Log.i(TAG, "[audio] evt=share_stop caller=${Throwable().stackTrace.getOrNull(1)}")
        AudioShare.stop()
        NativeHost.stop()
        projection?.unregisterCallback(projectionCallback)
        projection = null
        NativeHost.releaseProjection()
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun foregroundType(): Int =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION
        } else {
            0
        }

    private fun buildNotification(): Notification {
        getSystemService(NotificationManager::class.java).createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                getString(R.string.app_name),
                NotificationManager.IMPORTANCE_LOW,
            ),
        )

        val open =
            PendingIntent.getActivity(
                this,
                0,
                Intent(this, MainActivity::class.java),
                PendingIntent.FLAG_IMMUTABLE,
            )

        return NotificationCompat
            .Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(NativeClient.string(NativeClient.STR_SHARING_TITLE))
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setOngoing(true)
            .setContentIntent(open)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun readConsent(intent: Intent): Intent? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(EXTRA_CONSENT, Intent::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(EXTRA_CONSENT)
        }

    data class ShareRequest(
        val fps: Int,
        val bitrateMbps: Int,
        val maxDim: Int,
        val port: Int,
    ) {
        companion object {
            fun from(intent: Intent) =
                ShareRequest(
                    fps = intent.getIntExtra(EXTRA_FPS, 0),
                    bitrateMbps = intent.getIntExtra(EXTRA_BITRATE, 0),
                    maxDim = intent.getIntExtra(EXTRA_MAX_DIM, 0),
                    port = intent.getIntExtra(EXTRA_PORT, 0),
                )
        }
    }

    companion object {
        private const val CHANNEL_ID = "deskhub-sharing"
        private const val NOTIFICATION_ID = 1
        private const val EXTRA_RESULT_CODE = "resultCode"
        private const val EXTRA_CONSENT = "consent"
        private const val EXTRA_FPS = "fps"
        private const val EXTRA_BITRATE = "bitrateMbps"
        private const val EXTRA_MAX_DIM = "maxDim"
        private const val EXTRA_PORT = "port"
        private const val ACTION_STOP = "com.deskhub.app.STOP_SHARING"

        fun start(
            context: Context,
            resultCode: Int,
            consent: Intent,
            request: ShareRequest,
        ) {
            val intent =
                Intent(context, HostService::class.java)
                    .putExtra(EXTRA_RESULT_CODE, resultCode)
                    .putExtra(EXTRA_CONSENT, consent)
                    .putExtra(EXTRA_FPS, request.fps)
                    .putExtra(EXTRA_BITRATE, request.bitrateMbps)
                    .putExtra(EXTRA_MAX_DIM, request.maxDim)
                    .putExtra(EXTRA_PORT, request.port)
            ContextCompat.startForegroundService(context, intent)
        }

        fun stop(context: Context) {
            context.startService(
                Intent(context, HostService::class.java).setAction(ACTION_STOP),
            )
        }
    }
}
