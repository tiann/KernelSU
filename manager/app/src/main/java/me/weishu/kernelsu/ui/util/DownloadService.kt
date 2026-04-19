package me.weishu.kernelsu.ui.util

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.MainActivity
import okhttp3.Request
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.ConcurrentHashMap

class DownloadService : Service() {

    companion object {
        const val CHANNEL_ID = "download_channel"
        const val ACTION_DOWNLOAD = "me.weishu.kernelsu.action.DOWNLOAD"
        const val ACTION_CANCEL = "me.weishu.kernelsu.action.CANCEL_DOWNLOAD"
        const val ACTION_DISMISS_DOWNLOAD = "me.weishu.kernelsu.action.DISMISS_DOWNLOAD"
        const val ACTION_INSTALL_MODULE = "me.weishu.kernelsu.action.INSTALL_MODULE"
        const val EXTRA_URL = "url"
        const val EXTRA_FILE_NAME = "fileName"
        const val EXTRA_DOWNLOAD_ID = "downloadId"
        const val EXTRA_MODULE_URI = "moduleUri"
        const val EXTRA_FILE_PATH = "filePath"

        private const val COMPLETION_NOTIFICATION_ID_BASE = 100000
    }

    private val activeJobs = ConcurrentHashMap<Int, Job>()
    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private lateinit var notificationManager: NotificationManager

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        notificationManager = getSystemService(NotificationManager::class.java)
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_DOWNLOAD -> {
                val url = intent.getStringExtra(EXTRA_URL) ?: return START_NOT_STICKY
                val fileName = intent.getStringExtra(EXTRA_FILE_NAME) ?: return START_NOT_STICKY
                val downloadId = intent.getIntExtra(EXTRA_DOWNLOAD_ID, -1)
                if (downloadId == -1) return START_NOT_STICKY

                val notification = buildProgressNotification(downloadId, fileName, 0)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                    startForeground(
                        downloadId, notification,
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC
                    )
                } else {
                    startForeground(downloadId, notification)
                }

                startDownload(downloadId, url, fileName)
            }

            ACTION_CANCEL -> {
                val downloadId = intent.getIntExtra(EXTRA_DOWNLOAD_ID, -1)
                if (downloadId != -1) {
                    activeJobs[downloadId]?.cancel()
                    activeJobs.remove(downloadId)
                    notificationManager.cancel(downloadId)
                    DownloadManager.markFailed(downloadId, "Cancelled")
                    stopForegroundIfIdle()
                }
            }

            ACTION_DISMISS_DOWNLOAD -> {
                val downloadId = intent.getIntExtra(EXTRA_DOWNLOAD_ID, -1)
                val filePath = intent.getStringExtra(EXTRA_FILE_PATH)
                if (downloadId != -1) {
                    notificationManager.cancel(COMPLETION_NOTIFICATION_ID_BASE + downloadId)
                }
                if (!filePath.isNullOrEmpty()) {
                    File(filePath).delete()
                }
            }
        }
        return START_NOT_STICKY
    }

    private fun startDownload(id: Int, url: String, fileName: String) {
        val job = serviceScope.launch {
            val target = resolveAvailableTarget(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                fileName
            )
            try {
                ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute()
                    .use { resp ->
                        if (!resp.isSuccessful) throw IOException("HTTP ${resp.code}")
                        val body = resp.body
                        val total = body.contentLength()

                        FileOutputStream(target).use { fos ->
                            val buf = ByteArray(8 * 1024)
                            var soFar = 0L
                            var lastNotifiedProgress = -1
                            val source = body.byteStream()

                            while (true) {
                                val read = source.read(buf)
                                if (read == -1) break
                                fos.write(buf, 0, read)
                                soFar += read

                                if (total > 0) {
                                    val percent =
                                        ((soFar * 100L) / total).toInt().coerceIn(0, 100)
                                    DownloadManager.updateProgress(id, percent)

                                    if (percent - lastNotifiedProgress >= 2 || percent == 100) {
                                        notificationManager.notify(
                                            id,
                                            buildProgressNotification(id, target.name, percent)
                                        )
                                        lastNotifiedProgress = percent
                                    }
                                }
                            }
                            fos.flush()
                        }
                    }

                val uri = Uri.fromFile(target)
                DownloadManager.markCompleted(id, uri)

                notificationManager.cancel(id)
                notificationManager.notify(
                    COMPLETION_NOTIFICATION_ID_BASE + id,
                    buildCompletionNotification(id, target.name, uri)
                )
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                DownloadManager.markFailed(id, e.message ?: "Unknown error")

                notificationManager.cancel(id)
                notificationManager.notify(
                    COMPLETION_NOTIFICATION_ID_BASE + id,
                    buildFailureNotification(target.name)
                )
            } finally {
                activeJobs.remove(id)
                stopForegroundIfIdle()
            }
        }
        activeJobs[id] = job
    }

    private fun resolveAvailableTarget(
        directory: File,
        fileName: String
    ): File {
        val dotIndex = fileName.lastIndexOf('.')
        val baseName = if (dotIndex > 0) fileName.substring(0, dotIndex) else fileName
        val extension = if (dotIndex > 0) fileName.substring(dotIndex) else ""

        var index = 0
        while (true) {
            val candidateName = if (index == 0) {
                fileName
            } else {
                "$baseName ($index)$extension"
            }
            val candidate = File(directory, candidateName)
            if (!candidate.exists()) {
                return candidate
            }
            index++
        }
    }

    private fun buildProgressNotification(
        id: Int,
        fileName: String,
        progress: Int
    ) = NotificationCompat.Builder(this, CHANNEL_ID)
        .setContentTitle(getString(R.string.download_progress_title, fileName))
        .setContentText("$progress%")
        .setSmallIcon(android.R.drawable.stat_sys_download)
        .setProgress(100, progress, progress == 0)
        .setOngoing(true)
        .setSilent(true)
        .addAction(
            android.R.drawable.ic_menu_close_clear_cancel,
            getString(R.string.download_cancel),
            createCancelPendingIntent(id)
        )
        .build()

    private fun buildCompletionNotification(
        id: Int,
        fileName: String,
        uri: Uri
    ): android.app.Notification {
        val builder = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.download_complete_title))
            .setContentText(getString(R.string.download_complete_content, fileName))
            .setSmallIcon(android.R.drawable.stat_sys_download_done)
            .setAutoCancel(true)

        // Add "Install" action button
        val installIntent = Intent(this, MainActivity::class.java).apply {
            action = ACTION_INSTALL_MODULE
            putExtra(EXTRA_MODULE_URI, uri.toString())
            putExtra(EXTRA_DOWNLOAD_ID, id)
            addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        val installPendingIntent = PendingIntent.getActivity(
            this,
            id,
            installIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        builder.addAction(
            android.R.drawable.ic_menu_save,
            getString(R.string.download_install),
            installPendingIntent
        )
        builder.setContentIntent(installPendingIntent)

        // Add "Cancel" action button
        val dismissIntent = Intent(this, DownloadService::class.java).apply {
            action = ACTION_DISMISS_DOWNLOAD
            putExtra(EXTRA_DOWNLOAD_ID, id)
            putExtra(EXTRA_FILE_PATH, uri.path)
        }
        val dismissPendingIntent = PendingIntent.getService(
            this,
            COMPLETION_NOTIFICATION_ID_BASE + id,
            dismissIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        builder.addAction(
            android.R.drawable.ic_menu_close_clear_cancel,
            getString(R.string.download_cancel),
            dismissPendingIntent
        )

        return builder.build()
    }

    private fun buildFailureNotification(fileName: String) = NotificationCompat.Builder(this, CHANNEL_ID)
        .setContentTitle(getString(R.string.download_failed_title))
        .setContentText(getString(R.string.download_failed_content, fileName))
        .setSmallIcon(android.R.drawable.stat_notify_error)
        .setAutoCancel(true)
        .build()

    private fun createCancelPendingIntent(downloadId: Int): PendingIntent {
        val intent = Intent(this, DownloadService::class.java).apply {
            action = ACTION_CANCEL
            putExtra(EXTRA_DOWNLOAD_ID, downloadId)
        }
        return PendingIntent.getService(
            this,
            downloadId,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.download_channel_name),
            NotificationManager.IMPORTANCE_LOW
        )
        notificationManager.createNotificationChannel(channel)
    }

    private fun stopForegroundIfIdle() {
        if (activeJobs.isEmpty() || activeJobs.values.none { it.isActive }) {
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
        }
    }

    override fun onDestroy() {
        serviceScope.cancel()
        super.onDestroy()
    }
}
