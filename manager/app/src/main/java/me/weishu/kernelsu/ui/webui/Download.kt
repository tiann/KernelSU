package me.weishu.kernelsu.ui.webui

import java.io.InputStream
import java.io.OutputStream

private const val DEFAULT_WEBUI_DOWNLOAD_NAME = "download.bin"

internal fun sanitizeWebUIDownloadFileName(requestedFileName: String?): String {
    val trimmed = requestedFileName?.trim().orEmpty()
    if (trimmed.isEmpty()) return DEFAULT_WEBUI_DOWNLOAD_NAME

    val candidate = trimmed
        .substringAfterLast('/')
        .substringAfterLast('\\')
        .trim()

    if (candidate.isEmpty() || candidate == "." || candidate == "..") {
        return DEFAULT_WEBUI_DOWNLOAD_NAME
    }

    return candidate
}

internal fun writeWebUIDownload(
    source: InputStream,
    output: OutputStream,
    onBytesWritten: (Long) -> Unit = {},
): Long {
    output.buffered(64 * 1024).use { bufferedOutput ->
        val buffer = ByteArray(8 * 1024)
        var total = 0L
        while (true) {
            val read = source.read(buffer)
            if (read == -1) {
                break
            }
            bufferedOutput.write(buffer, 0, read)
            total += read
            onBytesWritten(total)
        }
        bufferedOutput.flush()
        return total
    }
}
