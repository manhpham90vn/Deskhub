package com.deskhub.app

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat

object AccessRequestNotifier {
    const val EXTRA_SECTION = "section"
    const val SECTION_DEVICES = "DEVICES"
    private const val CHANNEL_ID = "connection_requests"
    private const val OPEN_DEVICES_REQUEST_CODE = 3
    private const val UNSEEN_GENERATION = -1L

    private val announced = HashSet<String>()
    private var seenGeneration = UNSEEN_GENERATION

    fun poll(context: Context) {
        val generation = NativeClient.accessRequestsGeneration()
        if (generation == seenGeneration) return
        seenGeneration = generation
        announce(context, NativeClient.accessRequests())
    }

    fun announce(
        context: Context,
        requests: List<NativeClient.AccessRequest>,
    ) {
        val pending = requests.map { it.fingerprint }.toSet()
        announced.retainAll(pending)
        for (request in requests) {
            if (!announced.add(request.fingerprint)) continue
            post(context, request)
        }
    }

    private fun post(
        context: Context,
        request: NativeClient.AccessRequest,
    ) {
        if (!canPost(context)) return
        val title = NativeClient.string(NativeClient.STR_ACCESS_REQUEST_NOTIFICATION_TITLE)
        val body = NativeClient.accessRequestNotification(request.name, request.address)
        val manager = context.getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(CHANNEL_ID, title, NotificationManager.IMPORTANCE_HIGH),
        )
        manager.notify(
            request.fingerprint.hashCode(),
            NotificationCompat
                .Builder(context, CHANNEL_ID)
                .setSmallIcon(android.R.drawable.stat_notify_more)
                .setContentTitle(title)
                .setContentText(body)
                .setStyle(NotificationCompat.BigTextStyle().bigText(body))
                .setContentIntent(openDevicesPage(context))
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setAutoCancel(true)
                .build(),
        )
    }

    private fun canPost(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return true
        return context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED
    }

    private fun openDevicesPage(context: Context): PendingIntent =
        PendingIntent.getActivity(
            context,
            OPEN_DEVICES_REQUEST_CODE,
            Intent(context, MainActivity::class.java)
                .setPackage(context.packageName)
                .setAction(Intent.ACTION_MAIN)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
                .putExtra(EXTRA_SECTION, SECTION_DEVICES),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
}
