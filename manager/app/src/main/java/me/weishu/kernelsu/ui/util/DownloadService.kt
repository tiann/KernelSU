package me.weishu.kernelsu.ui.util

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.ContentValues
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.IBinder
import android.provider.MediaStore
import android.util.Log
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.registry.DownloadVerificationException
import me.weishu.kernelsu.data.registry.ExpectedFile
import me.weishu.kernelsu.data.registry.RegistryDownloadStore
import me.weishu.kernelsu.data.registry.RegistryUrls
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.MainActivity
import okhttp3.Call
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.io.IOException
import java.net.URLConnection
import java.util.concurrent.ConcurrentHashMap

enum class DownloadCompletionAction {
    INSTALL_MODULE,
    OPEN_FILE,
}

internal fun resolveDownloadMimeType(fileName: String, providedMimeType: String?): String =
    providedMimeType?.trim()?.takeIf { it.isNotEmpty() }
        ?: URLConnection.guessContentTypeFromName(fileName)
        ?: "application/octet-stream"

class DownloadService : Service() {

    companion object {
        const val CHANNEL_ID = "download_channel"
        const val ACTION_DOWNLOAD = "me.weishu.kernelsu.action.DOWNLOAD"
        const val ACTION_CANCEL = "me.weishu.kernelsu.action.CANCEL_DOWNLOAD"
        const val ACTION_DISMISS_DOWNLOAD = "me.weishu.kernelsu.action.DISMISS_DOWNLOAD"
        const val ACTION_INSTALL_MODULE = "me.weishu.kernelsu.action.INSTALL_MODULE"
        const val EXTRA_URL = "url"
        const val EXTRA_TOKEN = "token"
        const val EXTRA_FILE_NAME = "fileName"
        const val EXTRA_DOWNLOAD_ID = "downloadId"
        const val EXTRA_MODULE_URI = "moduleUri"
        const val EXTRA_EXPECTED_SIZE = "expectedSize"
        const val EXTRA_EXPECTED_SHA256 = "expectedSha256"
        const val EXTRA_TARGET_PATH = "targetPath"
        const val EXTRA_MIME_TYPE = "mimeType"
        const val EXTRA_COOKIE = "cookie"
        const val EXTRA_USER_AGENT = "userAgent"
        const val EXTRA_COMPLETION_ACTION = "completionAction"
        const val EXTRA_DELETE_FILE_ON_DISMISS = "deleteFileOnDismiss"

        private const val COMPLETION_NOTIFICATION_ID_BASE = 100000
        private const val REGISTRY_DOWNLOAD_MAX_AGE_MS = 24L * 60 * 60 * 1000
    }

    private val activeJobs = ConcurrentHashMap<Int, Job>()
    private val activeCalls = ConcurrentHashMap<Int, Call>()
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
                val mimeType = intent.getStringExtra(EXTRA_MIME_TYPE)
                val cookie = intent.getStringExtra(EXTRA_COOKIE)
                val userAgent = intent.getStringExtra(EXTRA_USER_AGENT)
                val completionAction = intent.getStringExtra(EXTRA_COMPLETION_ACTION)
                    ?.let { runCatching { DownloadCompletionAction.valueOf(it) }.getOrNull() }
                    ?: DownloadCompletionAction.INSTALL_MODULE
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

                val expectedSha256 = intent.getStringExtra(EXTRA_EXPECTED_SHA256)
                val expectedSize = intent.getLongExtra(EXTRA_EXPECTED_SIZE, -1L)
                val expected = expectedSha256?.let { ExpectedFile(expectedSize, it) }
                startDownload(downloadId, url, fileName, mimeType, cookie, userAgent, completionAction, expected)
            }

            ACTION_CANCEL -> {
                val downloadId = intent.getIntExtra(EXTRA_DOWNLOAD_ID, -1)
                if (downloadId != -1) {
                    activeJobs[downloadId]?.cancel()
                    activeCalls[downloadId]?.cancel()
                }
            }

            ACTION_DISMISS_DOWNLOAD -> {
                val downloadId = intent.getIntExtra(EXTRA_DOWNLOAD_ID, -1)
                val uri = intent.getStringExtra(EXTRA_MODULE_URI)?.let(Uri::parse)
                val deleteFileOnDismiss = intent.getBooleanExtra(EXTRA_DELETE_FILE_ON_DISMISS, true)
                if (downloadId != -1) {
                    notificationManager.cancel(COMPLETION_NOTIFICATION_ID_BASE + downloadId)
                }
                serviceScope.launch {
                    if (deleteFileOnDismiss && uri != null) deleteDownload(uri)
                    stopForegroundIfIdle()
                }
            }
        }
        return START_NOT_STICKY
    }

    /**
     * Registry downloads ([expected] != null) may only come from GitHub Release download URLs
     * and their CDN redirect targets; every hop is checked.
     */
    private fun registryClient(): OkHttpClient = ksuApp.okhttpClient.newBuilder()
        .followSslRedirects(false)
        .addNetworkInterceptor { chain ->
            val requestUrl = chain.request().url
            if (!RegistryUrls.isAllowedDownloadHop(requestUrl.toString())) {
                throw DownloadVerificationException(
                    DownloadVerificationException.Reason.URL_NOT_ALLOWED,
                    "Download host not allowed: ${requestUrl.host}"
                )
            }
            chain.proceed(chain.request())
        }
        .build()

    private fun startDownload(
        id: Int,
        url: String,
        fileName: String,
        mimeType: String?,
        cookie: String?,
        userAgent: String?,
        completionAction: DownloadCompletionAction,
        expected: ExpectedFile?,
    ) {
        val job = serviceScope.launch(start = CoroutineStart.LAZY) {
            var pendingUri: Uri? = null
            var displayName = fileName
            try {
                val uri = if (expected != null) {
                    // Already stored under its final name: delete it if cancelled before completion.
                    downloadVerified(id, url, fileName, expected).also { pendingUri = it }
                } else {
                    val call = ksuApp.okhttpClient.newCall(Request.Builder().url(url).apply {
                        cookie?.takeIf { it.isNotEmpty() }?.let { addHeader("Cookie", it) }
                        userAgent?.takeIf { it.isNotEmpty() }?.let { addHeader("User-Agent", it) }
                    }.build())
                    activeCalls[id] = call
                    ensureActive()
                    val published = call.execute()
                        .use { resp ->
                        if (!resp.isSuccessful) throw IOException("HTTP ${resp.code}")
                        val body = resp.body
                        val total = body.contentLength()

                        ensureActive()
                        val values = ContentValues().apply {
                            put(MediaStore.Downloads.DISPLAY_NAME, fileName)
                            put(MediaStore.Downloads.MIME_TYPE, resolveDownloadMimeType(fileName, mimeType))
                            put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
                            put(MediaStore.Downloads.IS_PENDING, 1)
                        }
                        val downloadUri = contentResolver.insert(
                            MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY),
                            values
                        ) ?: throw IOException("Failed to create download")
                        pendingUri = downloadUri
                        displayName = downloadUri.getFileName(this@DownloadService) ?: fileName

                        val output = contentResolver.openOutputStream(downloadUri)
                            ?: throw IOException("Failed to open download")
                        output.use { fos ->
                            val buf = ByteArray(8 * 1024)
                            var soFar = 0L
                            val progress = ProgressReporter(id, displayName, total)
                            val source = body.byteStream()

                            while (true) {
                                ensureActive()
                                val read = source.read(buf)
                                if (read == -1) break
                                fos.write(buf, 0, read)
                                soFar += read
                                progress.report(soFar)
                            }
                            fos.flush()
                        }
                        downloadUri
                    }

                    ensureActive()
                    val values = ContentValues().apply {
                        put(MediaStore.Downloads.IS_PENDING, 0)
                    }
                    if (contentResolver.update(published, values, null, null) != 1) {
                        throw IOException("Failed to publish download")
                    }
                    displayName = published.getFileName(this@DownloadService) ?: displayName
                    published
                }
                ensureActive()
                pendingUri = null
                DownloadManager.markCompleted(id, uri)

                notificationManager.cancel(id)
                notificationManager.notify(
                    COMPLETION_NOTIFICATION_ID_BASE + id,
                    buildCompletionNotification(id, displayName, uri, mimeType, completionAction)
                )
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                ensureActive()
                val verificationFailure = (e as? DownloadVerificationException)?.reason
                if (verificationFailure != null) {
                    Log.w("DownloadService", "Rejected download $url: ${e.message}")
                }
                DownloadManager.markFailed(id, e.message ?: "Unknown error", verificationFailure)

                notificationManager.cancel(id)
                notificationManager.notify(
                    COMPLETION_NOTIFICATION_ID_BASE + id,
                    buildFailureNotification(displayName, verificationFailure)
                )
            } finally {
                pendingUri?.let { deleteDownload(it) }
            }
        }
        activeJobs[id] = job
        job.invokeOnCompletion { cause ->
            if (cause is CancellationException &&
                DownloadManager.downloads.value[id]?.status != DownloadManager.Status.COMPLETED
            ) {
                DownloadManager.markFailed(id, "Cancelled")
                notificationManager.cancel(id)
            }
            activeCalls.remove(id)?.cancel()
            activeJobs.remove(id)
            stopForegroundIfIdle()
        }
        job.start()
    }

    private inner class ProgressReporter(private val id: Int, private val displayName: String, private val total: Long) {
        private var lastNotifiedProgress = -1

        fun report(soFar: Long) {
            if (total <= 0) return
            val percent = ((soFar * 100L) / total).toInt().coerceIn(0, 100)
            DownloadManager.updateProgress(id, percent)
            if (percent - lastNotifiedProgress >= 2 || percent == 100) {
                notificationManager.notify(id, buildProgressNotification(id, displayName, percent))
                lastNotifiedProgress = percent
            }
        }
    }

    private fun registryStoreRoot(): File = File(filesDir, RegistryDownloadStore.DIR_NAME)

    /**
     * Registry release ZIP: only GitHub Release URLs (and their CDN redirect targets), streamed
     * into app-private storage while size + SHA-256 are checked. A mismatching file is deleted
     * and never published; the flash step re-verifies the stored bytes before installing.
     */
    private fun CoroutineScope.downloadVerified(id: Int, url: String, fileName: String, expected: ExpectedFile): Uri {
        if (!RegistryUrls.isAllowedReleaseDownloadUrl(url)) {
            throw DownloadVerificationException(
                DownloadVerificationException.Reason.URL_NOT_ALLOWED,
                "Download URL not allowed"
            )
        }
        val root = registryStoreRoot()
        runCatching { RegistryDownloadStore.cleanup(root, REGISTRY_DOWNLOAD_MAX_AGE_MS) }
        val call = registryClient().newCall(Request.Builder().url(url).build())
        activeCalls[id] = call
        ensureActive()
        val file = call.execute().use { resp ->
            if (!resp.isSuccessful) throw IOException("HTTP ${resp.code}")
            val progress = ProgressReporter(id, fileName, expected.size)
            RegistryDownloadStore.download(
                input = resp.body.byteStream(),
                root = root,
                expected = expected,
                fileName = fileName,
                onProgress = progress::report,
                checkActive = { ensureActive() },
            )
        }
        return Uri.fromFile(file)
    }

    private fun deleteDownload(uri: Uri) {
        runCatching {
            if (uri.scheme == "file") {
                uri.path?.let { RegistryDownloadStore.delete(registryStoreRoot(), File(it)) }
                return@runCatching
            }
            contentResolver.delete(uri, null, null)
        }.onFailure {
            Log.w("DownloadService", "Failed to delete download: $uri", it)
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
        uri: Uri,
        mimeType: String?,
        completionAction: DownloadCompletionAction,
    ): android.app.Notification {
        val builder = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.download_complete_title))
            .setContentText(getString(R.string.download_complete_content, fileName))
            .setSmallIcon(android.R.drawable.stat_sys_download_done)
            .setAutoCancel(true)

        val primaryPendingIntent = when (completionAction) {
            DownloadCompletionAction.INSTALL_MODULE -> PendingIntent.getActivity(
                this, id, Intent(this, MainActivity::class.java).apply {
                    action = ACTION_INSTALL_MODULE
                    putExtra(EXTRA_MODULE_URI, uri.toString())
                    putExtra(EXTRA_DOWNLOAD_ID, id)
                    putExtra(EXTRA_TOKEN, SettingsRepositoryImpl().intentToken)
                    addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                }, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            ).also { builder.addAction(android.R.drawable.ic_menu_save, getString(R.string.download_install), it) }
            DownloadCompletionAction.OPEN_FILE -> PendingIntent.getActivity(
                this, id, Intent.createChooser(Intent(Intent.ACTION_VIEW).apply {
                    setDataAndType(uri, resolveDownloadMimeType(fileName, mimeType))
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
                }, getString(R.string.open)), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            ).also { builder.addAction(android.R.drawable.ic_menu_view, getString(R.string.open), it) }
        }
        builder.setContentIntent(primaryPendingIntent)

        // Add "Cancel" action button
        val dismissIntent = Intent(this, DownloadService::class.java).apply {
            action = ACTION_DISMISS_DOWNLOAD
            putExtra(EXTRA_DOWNLOAD_ID, id)
            putExtra(EXTRA_MODULE_URI, uri.toString())
            putExtra(EXTRA_DELETE_FILE_ON_DISMISS, completionAction == DownloadCompletionAction.INSTALL_MODULE)
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

    private fun buildFailureNotification(
        fileName: String,
        verificationFailure: DownloadVerificationException.Reason? = null,
    ) = NotificationCompat.Builder(this, CHANNEL_ID)
        .setContentTitle(getString(R.string.download_failed_title))
        .setContentText(
            when (verificationFailure) {
                null -> getString(R.string.download_failed_content, fileName)
                DownloadVerificationException.Reason.URL_NOT_ALLOWED -> getString(R.string.module_repo_download_url_blocked)
                DownloadVerificationException.Reason.SIZE_MISMATCH,
                DownloadVerificationException.Reason.SHA256_MISMATCH ->
                    getString(R.string.module_repo_download_verify_failed, fileName)
            }
        )
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
        if (activeJobs.isEmpty()) {
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
        }
    }

    override fun onDestroy() {
        serviceScope.cancel()
        activeCalls.values.forEach { it.cancel() }
        super.onDestroy()
    }
}
