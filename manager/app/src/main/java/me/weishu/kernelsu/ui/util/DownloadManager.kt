package me.weishu.kernelsu.ui.util

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Handler
import android.os.Looper
import androidx.core.content.ContextCompat
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import me.weishu.kernelsu.data.registry.DownloadVerificationException
import me.weishu.kernelsu.data.registry.ExpectedFile
import me.weishu.kernelsu.data.registry.RegistryUrls
import me.weishu.kernelsu.data.registry.canShareDownload
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

object DownloadManager {

    enum class Status { PENDING, DOWNLOADING, COMPLETED, FAILED }

    data class DownloadState(
        val id: Int,
        val fileName: String,
        val url: String,
        val progress: Int = 0,
        val status: Status = Status.PENDING,
        val resultUri: Uri? = null,
        val error: String? = null,
        /** Set when a registry download was rejected (URL allowlist, size or SHA-256 mismatch). */
        val verificationFailure: DownloadVerificationException.Reason? = null,
        /** Size + SHA-256 a registry download is verified against; null for plain downloads. */
        val expected: ExpectedFile? = null,
    )

    private val idCounter = AtomicInteger(0)
    private val _downloads = MutableStateFlow<Map<Int, DownloadState>>(emptyMap())
    val downloads: StateFlow<Map<Int, DownloadState>> = _downloads.asStateFlow()

    private val completionCallbacks = ConcurrentHashMap<Int, (Uri) -> Unit>()
    private val mainHandler = Handler(Looper.getMainLooper())

    fun enqueue(
        context: Context,
        url: String,
        fileName: String,
        onCompleted: ((Uri) -> Unit)? = null,
        expected: ExpectedFile? = null,
    ): Int {
        // Only share an in-flight download that is verified exactly like this request.
        val existing = _downloads.value.values.find {
            (it.status == Status.PENDING || it.status == Status.DOWNLOADING) &&
                canShareDownload(url, expected, it.url, it.expected)
        }
        if (existing != null) {
            if (onCompleted != null) {
                completionCallbacks.merge(existing.id, onCompleted) { a, b -> { uri -> a(uri); b(uri) } }
                // It may have finished in between; markCompleted / markFailed already drained the
                // callbacks then, so run (completed) or drop (failed) what was merged afterwards.
                val now = _downloads.value[existing.id]
                if (now?.status == Status.COMPLETED && now.resultUri != null) {
                    completionCallbacks.remove(existing.id)?.let { cb -> mainHandler.post { cb(now.resultUri) } }
                } else if (now == null || now.status == Status.FAILED) {
                    completionCallbacks.remove(existing.id)
                }
            }
            return existing.id
        }

        val id = idCounter.incrementAndGet()
        val state = DownloadState(id = id, fileName = fileName, url = url, expected = expected)
        _downloads.update { it + (id to state) }

        if (expected != null && !RegistryUrls.isAllowedReleaseDownloadUrl(url)) {
            markFailed(id, "Download URL not allowed", DownloadVerificationException.Reason.URL_NOT_ALLOWED)
            return id
        }

        if (onCompleted != null) {
            completionCallbacks[id] = onCompleted
        }

        val intent = Intent(context, DownloadService::class.java).apply {
            action = DownloadService.ACTION_DOWNLOAD
            putExtra(DownloadService.EXTRA_DOWNLOAD_ID, id)
            putExtra(DownloadService.EXTRA_URL, url)
            putExtra(DownloadService.EXTRA_FILE_NAME, fileName)
            if (expected != null) {
                putExtra(DownloadService.EXTRA_EXPECTED_SIZE, expected.size)
                putExtra(DownloadService.EXTRA_EXPECTED_SHA256, expected.sha256)
            }
        }
        ContextCompat.startForegroundService(context, intent)

        return id
    }

    internal fun updateProgress(id: Int, progress: Int) {
        _downloads.update { map ->
            val state = map[id] ?: return@update map
            map + (id to state.copy(progress = progress, status = Status.DOWNLOADING))
        }
    }

    internal fun markCompleted(id: Int, uri: Uri) {
        _downloads.update { map ->
            val state = map[id] ?: return@update map
            map + (id to state.copy(status = Status.COMPLETED, progress = 100, resultUri = uri))
        }

        val callback = completionCallbacks.remove(id)
        if (callback != null) {
            mainHandler.post { callback(uri) }
        }
    }

    internal fun markFailed(
        id: Int,
        error: String,
        verificationFailure: DownloadVerificationException.Reason? = null,
    ) {
        _downloads.update { map ->
            val state = map[id] ?: return@update map
            map + (id to state.copy(status = Status.FAILED, error = error, verificationFailure = verificationFailure))
        }
        completionCallbacks.remove(id)
    }
}
