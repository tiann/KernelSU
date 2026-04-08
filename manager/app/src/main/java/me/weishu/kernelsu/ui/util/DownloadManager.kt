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
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

object DownloadManager {

    enum class Status { PENDING, DOWNLOADING, COMPLETED, FAILED }

    data class DownloadState(
        val id: Int,
        val fileName: String,
        val url: String,
        val targetPath: String? = null,
        val mimeType: String? = null,
        val completionAction: DownloadCompletionAction = DownloadCompletionAction.INSTALL_MODULE,
        val progress: Int = 0,
        val status: Status = Status.PENDING,
        val resultUri: Uri? = null,
        val error: String? = null,
    )

    private val idCounter = AtomicInteger(0)
    private val _downloads = MutableStateFlow<Map<Int, DownloadState>>(emptyMap())
    val downloads: StateFlow<Map<Int, DownloadState>> = _downloads.asStateFlow()

    private val completionCallbacks = ConcurrentHashMap<Int, (Uri) -> Unit>()
    private val mainHandler = Handler(Looper.getMainLooper())

    internal fun registerLocalSave(
        fileName: String,
        targetPath: String,
        mimeType: String? = null,
        completionAction: DownloadCompletionAction = DownloadCompletionAction.OPEN_FILE,
    ): Int {
        val id = idCounter.incrementAndGet()
        val state = DownloadState(
            id = id,
            fileName = fileName,
            url = targetPath,
            targetPath = targetPath,
            mimeType = mimeType,
            completionAction = completionAction,
        )
        _downloads.update { it + (id to state) }
        return id
    }

    fun enqueue(
        context: Context,
        url: String,
        fileName: String,
        targetPath: String? = null,
        mimeType: String? = null,
        cookie: String? = null,
        userAgent: String? = null,
        completionAction: DownloadCompletionAction = DownloadCompletionAction.INSTALL_MODULE,
        onCompleted: ((Uri) -> Unit)? = null,
    ): Int {
        val existing = _downloads.value.values.find {
            it.url == url &&
                it.fileName == fileName &&
                it.targetPath == targetPath &&
                it.completionAction == completionAction &&
                (it.status == Status.PENDING || it.status == Status.DOWNLOADING)
        }
        if (existing != null) return existing.id

        val id = idCounter.incrementAndGet()
        val state = DownloadState(
            id = id,
            fileName = fileName,
            url = url,
            targetPath = targetPath,
            mimeType = mimeType,
            completionAction = completionAction,
        )
        _downloads.update { it + (id to state) }

        if (onCompleted != null) {
            completionCallbacks[id] = onCompleted
        }

        val intent = Intent(context, DownloadService::class.java).apply {
            action = DownloadService.ACTION_DOWNLOAD
            putExtra(DownloadService.EXTRA_DOWNLOAD_ID, id)
            putExtra(DownloadService.EXTRA_URL, url)
            putExtra(DownloadService.EXTRA_FILE_NAME, fileName)
            putExtra(DownloadService.EXTRA_TARGET_PATH, targetPath)
            putExtra(DownloadService.EXTRA_MIME_TYPE, mimeType)
            putExtra(DownloadService.EXTRA_COOKIE, cookie)
            putExtra(DownloadService.EXTRA_USER_AGENT, userAgent)
            putExtra(DownloadService.EXTRA_COMPLETION_ACTION, completionAction.name)
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

    internal fun markFailed(id: Int, error: String) {
        _downloads.update { map ->
            val state = map[id] ?: return@update map
            map + (id to state.copy(status = Status.FAILED, error = error))
        }
        completionCallbacks.remove(id)
    }
}
