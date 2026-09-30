package com.deskhub.app

import android.Manifest
import android.app.Activity
import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

object FilesHost {
    private const val CHANNEL_ID = "deskhub-files"
    private const val NOTIFICATION_ID = 2
    private const val SYNC_INTERVAL_MS = 1_000L

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var app: Application? = null
    private var startedActivities = 0
    private var loop: Job? = null

    fun bind(application: Application) {
        if (app != null) return
        app = application
        application.registerActivityLifecycleCallbacks(
            object : Application.ActivityLifecycleCallbacks {
                override fun onActivityStarted(activity: Activity) {
                    startedActivities += 1
                    if (startedActivities == 1) enterForeground(application)
                }

                override fun onActivityStopped(activity: Activity) {
                    startedActivities -= 1
                    if (startedActivities == 0) leaveForeground()
                }

                override fun onActivityCreated(
                    activity: Activity,
                    savedInstanceState: Bundle?,
                ) = Unit

                override fun onActivityResumed(activity: Activity) = Unit

                override fun onActivityPaused(activity: Activity) = Unit

                override fun onActivitySaveInstanceState(
                    activity: Activity,
                    outState: Bundle,
                ) = Unit

                override fun onActivityDestroyed(activity: Activity) = Unit
            },
        )
    }

    private fun enterForeground(application: Application) {
        loop?.cancel()
        loop =
            scope.launch {
                while (true) {
                    sync(application)
                    AccessRequestNotifier.poll(application)
                    val delivered =
                        withContext(Dispatchers.IO) {
                            ReceivedFiles.exportCompleted(
                                application,
                                ReceivedFiles.transferDir(application),
                            )
                        }
                    if (delivered.isNotEmpty()) notifyArrived(application, delivered)
                    delay(SYNC_INTERVAL_MS)
                }
            }
    }

    private fun leaveForeground() {
        loop?.cancel()
        loop = null
        val screenBusy =
            NativeHost.shareState != NativeHost.ShareState.IDLE &&
                NativeHost.shareState != NativeHost.ShareState.FAILED
        if (screenBusy) return
        scope.launch { if (NativeHost.filesActive()) NativeHost.stopFiles() }
    }

    private suspend fun sync(context: Context) {
        val screenBusy =
            NativeHost.shareState != NativeHost.ShareState.IDLE &&
                NativeHost.shareState != NativeHost.ShareState.FAILED
        if (screenBusy || NativeHost.filesActive()) return
        NativeHost.startFiles(ReceivedFiles.transferDir(context))
    }

    private fun notifyArrived(
        context: Context,
        names: List<String>,
    ) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
            PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        val title = NativeClient.string(NativeClient.STR_TRANSFER_ARRIVED_TITLE)
        val manager = context.getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(CHANNEL_ID, title, NotificationManager.IMPORTANCE_DEFAULT),
        )
        val open =
            PendingIntent.getActivity(
                context,
                0,
                Intent(context, MainActivity::class.java)
                    .setPackage(context.packageName),
                PendingIntent.FLAG_IMMUTABLE,
            )
        manager.notify(
            NOTIFICATION_ID,
            NotificationCompat
                .Builder(context, CHANNEL_ID)
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setContentTitle(title)
                .setContentText(names.joinToString(", "))
                .setContentIntent(open)
                .setAutoCancel(true)
                .build(),
        )
    }
}
