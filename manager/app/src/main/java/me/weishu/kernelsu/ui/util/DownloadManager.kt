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

    fun enqueue(
        context: Context,
        url: String,
        fileName: String,
        onCompleted: ((Uri) -> Unit)? = null,
    ): Int {
        val existing = _downloads.value.values.find {
            it.url == url && (it.status == Status.PENDING || it.status == Status.DOWNLOADING)
        }
        if (existing != null) return existing.id

        val id = idCounter.incrementAndGet()
        val state = DownloadState(id = id, fileName = fileName, url = url)
        _downloads.update { it + (id to state) }

        if (onCompleted != null) {
            completionCallbacks[id] = onCompleted
        }

        val intent = Intent(context, DownloadService::class.java).apply {
            action = DownloadService.ACTION_DOWNLOAD
            putExtra(DownloadService.EXTRA_DOWNLOAD_ID, id)
            putExtra(DownloadService.EXTRA_URL, url)
            putExtra(DownloadService.EXTRA_FILE_NAME, fileName)
        }
        ContextCompat.startForegroundService(context, intent)

        return id
    }

    internal fun updateProgress(id: Int, progress: Int) {
        _downloads.update { map ->
            val state = map[id] ?: return@update map
            if (state.status == Status.COMPLETED || state.status == Status.FAILED) {
                return@update map
            }
            map + (id to state.copy(progress = progress, status = Status.DOWNLOADING))
        }
    }

    internal fun tryMarkCompleted(id: Int, uri: Uri): Boolean {
        if (!tryTransitionToTerminal(id) { state ->
                state.copy(status = Status.COMPLETED, progress = 100, resultUri = uri)
            }) {
            return false
        }

        val callback = completionCallbacks.remove(id)
        if (callback != null) {
            mainHandler.post { callback(uri) }
        }
        return true
    }

    internal fun tryMarkFailed(id: Int, error: String): Boolean {
        if (!tryTransitionToTerminal(id) { state ->
                state.copy(status = Status.FAILED, error = error)
            }) {
            return false
        }
        completionCallbacks.remove(id)
        return true
    }

    private inline fun tryTransitionToTerminal(
        id: Int,
        transform: (DownloadState) -> DownloadState,
    ): Boolean {
        while (true) {
            val current = _downloads.value
            val state = current[id] ?: return false
            if (state.status == Status.COMPLETED || state.status == Status.FAILED) {
                return false
            }
            val updated = current + (id to transform(state))
            if (_downloads.compareAndSet(current, updated)) {
                return true
            }
        }
    }
}
