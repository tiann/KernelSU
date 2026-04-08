package me.weishu.kernelsu.ui.webui

import android.util.Log
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import java.io.ByteArrayInputStream
import java.io.PipedInputStream
import java.io.PipedOutputStream
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit

/**
 * Handles blob downloads by providing a streaming interface instead of loading
 * entire files into memory as base64 strings.
 * 
 * Thread safety: All public methods are thread-safe. The cleanup() method must be called
 * when the WebUI is disposed to prevent resource leaks.
 */
object BlobDownloadHandler {
    private const val BLOB_DOWNLOAD_HOST = "blob-download.kernelsu.internal"
    private const val CHUNK_SIZE = 64 * 1024
    private const val PIPE_SIZE = 256 * 1024
    private const val STALE_THRESHOLD_MS = 5 * 60 * 1000L // 5 minutes

    private val pendingDownloads = ConcurrentHashMap<String, PendingDownload>()
    @Volatile
    private var lastCleanupTime = System.currentTimeMillis()

    data class PendingDownload(
        val fileName: String,
        val mimeType: String?,
        val inputStream: PipedInputStream,
        val outputStream: PipedOutputStream,
        val createdAt: Long = System.currentTimeMillis()
    )

    /**
     * Register a new blob download session and return an internal URL
     * that can be used to stream the data.
     */
    fun registerBlobDownload(fileName: String, mimeType: String?): String {
        val downloadId = UUID.randomUUID().toString()
        val inputStream = PipedInputStream(PIPE_SIZE)
        val outputStream = PipedOutputStream(inputStream)

        pendingDownloads[downloadId] = PendingDownload(
            fileName = fileName,
            mimeType = mimeType,
            inputStream = inputStream,
            outputStream = outputStream
        )

        // Clean up old pending downloads periodically (not on every call to avoid overhead)
        val now = System.currentTimeMillis()
        if (now - lastCleanupTime > STALE_THRESHOLD_MS) {
            lastCleanupTime = now
            cleanupStaleDownloads()
        }

        return "https://$BLOB_DOWNLOAD_HOST/$downloadId"
    }

    /**
     * Write a chunk of data to the registered download session.
     * Returns true if successful, false if the session doesn't exist.
     */
    fun writeChunk(downloadId: String, base64Chunk: String): Boolean {
        val download = pendingDownloads[downloadId] ?: return false

        return try {
            val decoded = android.util.Base64.decode(base64Chunk, android.util.Base64.DEFAULT)
            download.outputStream.write(decoded)
            download.outputStream.flush()
            true
        } catch (e: Exception) {
            Log.e("BlobDownloadHandler", "Failed to write chunk for $downloadId", e)
            closeDownload(downloadId)
            false
        }
    }

    /**
     * Mark the download as complete and close the output stream.
     */
    fun completeDownload(downloadId: String): Boolean {
        val download = pendingDownloads[downloadId] ?: return false

        return try {
            download.outputStream.flush()
            download.outputStream.close()
            true
        } catch (e: Exception) {
            Log.e("BlobDownloadHandler", "Failed to complete download $downloadId", e)
            closeDownload(downloadId)
            false
        }
    }

    /**
     * Cancel a download and clean up resources.
     */
    fun cancelDownload(downloadId: String) {
        closeDownload(downloadId)
    }

    private fun closeDownload(downloadId: String) {
        pendingDownloads.remove(downloadId)?.let { download ->
            runCatching { download.outputStream.close() }
            runCatching { download.inputStream.close() }
        }
    }

    /**
     * Intercept requests to the internal blob download host and return
     * the streaming response.
     */
    fun shouldInterceptRequest(request: WebResourceRequest): WebResourceResponse? {
        val url = request.url
        if (url.scheme != "https" || url.host != BLOB_DOWNLOAD_HOST) {
            return null
        }

        val downloadId = url.path?.substring(1) ?: return createErrorResponse("Invalid download ID")
        val download = pendingDownloads[downloadId] ?: return createErrorResponse("Download not found")

        return WebResourceResponse(
            download.mimeType ?: "application/octet-stream",
            null,
            download.inputStream
        ).apply {
            responseHeaders = mapOf(
                "Content-Disposition" to "attachment; filename=\"${download.fileName}\"",
                "Access-Control-Allow-Origin" to "*"
            )
        }
    }

    private fun createErrorResponse(message: String): WebResourceResponse {
        return WebResourceResponse(
            "text/plain",
            "utf-8",
            404,
            "Not Found",
            emptyMap(),
            ByteArrayInputStream(message.toByteArray(Charsets.UTF_8))
        )
    }

    private fun cleanupStaleDownloads() {
        val now = System.currentTimeMillis()
        val staleThreshold = TimeUnit.MINUTES.toMillis(5)

        pendingDownloads.entries.removeIf { (downloadId, download) ->
            if (now - download.createdAt > staleThreshold) {
                Log.w("BlobDownloadHandler", "Cleaning up stale download: $downloadId")
                closeDownload(downloadId)
                true
            } else {
                false
            }
        }
    }

    fun cleanup() {
        pendingDownloads.keys.forEach { closeDownload(it) }
    }
}
