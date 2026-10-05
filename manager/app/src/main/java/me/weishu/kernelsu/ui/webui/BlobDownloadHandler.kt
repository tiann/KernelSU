package me.weishu.kernelsu.ui.webui

import android.util.Base64
import android.util.Log
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FilterInputStream
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit

/**
 * Handles blob downloads by spooling chunks to a temporary file before WebView
 * requests the generated internal URL.
 */
object BlobDownloadHandler {
    private const val BLOB_DOWNLOAD_HOST = "blob-download.kernelsu.internal"
    private const val STALE_THRESHOLD_MS = 5 * 60 * 1000L

    private val pendingDownloads = ConcurrentHashMap<String, PendingDownload>()
    @Volatile
    private var lastCleanupTime = System.currentTimeMillis()

    private data class PendingDownload(
        val fileName: String,
        val mimeType: String?,
        val tempFile: File,
        val outputStream: OutputStream,
        val createdAt: Long = System.currentTimeMillis(),
        @Volatile var completed: Boolean = false,
    )

    /** Register a new blob download session and return its internal URL. */
    fun registerBlobDownload(fileName: String, mimeType: String?, cacheDir: File): String {
        val downloadId = UUID.randomUUID().toString()
        val tempFile = File.createTempFile("ksu-blob-", ".download", cacheDir)
        pendingDownloads[downloadId] = PendingDownload(
            fileName = fileName,
            mimeType = mimeType,
            tempFile = tempFile,
            outputStream = tempFile.outputStream().buffered(64 * 1024),
        )

        val now = System.currentTimeMillis()
        if (now - lastCleanupTime > STALE_THRESHOLD_MS) {
            lastCleanupTime = now
            cleanupStaleDownloads()
        }

        return "https://$BLOB_DOWNLOAD_HOST/$downloadId"
    }

    /** Write one base64 encoded chunk to the session. */
    fun writeChunk(downloadId: String, base64Chunk: String): Boolean {
        val download = pendingDownloads[downloadId] ?: return false
        return try {
            synchronized(download) {
                if (download.completed) return false
                download.outputStream.write(Base64.decode(base64Chunk, Base64.DEFAULT))
            }
            true
        } catch (e: Exception) {
            Log.e("BlobDownloadHandler", "Failed to write chunk for $downloadId", e)
            closeDownload(downloadId)
            false
        }
    }

    /** Mark the download complete and make the spooled file available to WebView. */
    fun completeDownload(downloadId: String): Boolean {
        val download = pendingDownloads[downloadId] ?: return false
        return try {
            synchronized(download) {
                if (download.completed) return false
                download.outputStream.flush()
                download.outputStream.close()
                download.completed = true
            }
            true
        } catch (e: Exception) {
            Log.e("BlobDownloadHandler", "Failed to complete download $downloadId", e)
            closeDownload(downloadId)
            false
        }
    }

    fun cancelDownload(downloadId: String) = closeDownload(downloadId)

    /** Intercept requests to the internal blob URL and serve the completed file. */
    fun shouldInterceptRequest(request: WebResourceRequest): WebResourceResponse? {
        val url = request.url
        if (url.scheme != "https" || url.host != BLOB_DOWNLOAD_HOST) return null

        val downloadId = url.path?.substring(1)?.takeIf { it.isNotEmpty() }
            ?: return createErrorResponse("Invalid download ID")
        val download = pendingDownloads[downloadId]
            ?: return createErrorResponse("Download not found")

        if (!download.completed) return createErrorResponse("Download is not ready")
        val input = runCatching { FileInputStream(download.tempFile) }.getOrElse {
            closeDownload(downloadId)
            return createErrorResponse("Download not found")
        }

        return WebResourceResponse(
            download.mimeType ?: "application/octet-stream",
            null,
            CleanupInputStream(input) { closeDownload(downloadId) },
        ).apply {
            responseHeaders = mapOf(
                "Content-Disposition" to "attachment; filename=\"${download.fileName}\"",
                "Access-Control-Allow-Origin" to "*",
            )
        }
    }

    private fun closeDownload(downloadId: String) {
        pendingDownloads.remove(downloadId)?.let { download ->
            runCatching { download.outputStream.close() }
            runCatching { download.tempFile.delete() }
        }
    }

    private fun createErrorResponse(message: String) = WebResourceResponse(
        "text/plain",
        "utf-8",
        404,
        "Not Found",
        emptyMap(),
        ByteArrayInputStream(message.toByteArray(Charsets.UTF_8)),
    )

    private fun cleanupStaleDownloads() {
        val now = System.currentTimeMillis()
        val staleIds = pendingDownloads.entries
            .filter { now - it.value.createdAt > TimeUnit.MINUTES.toMillis(5) }
            .map { it.key }
        staleIds.forEach { downloadId ->
            Log.w("BlobDownloadHandler", "Cleaning up stale download: $downloadId")
            closeDownload(downloadId)
        }
    }

    fun cleanup() = pendingDownloads.keys.toList().forEach(::closeDownload)

    private class CleanupInputStream(
        input: InputStream,
        private val onClose: () -> Unit,
    ) : FilterInputStream(input) {
        override fun close() {
            runCatching { super.close() }
            onClose()
        }
    }
}
