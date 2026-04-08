package me.weishu.kernelsu.ui.webui

import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream

private const val DEFAULT_WEBUI_DOWNLOAD_NAME = "download.bin"

internal fun resolveWebUIDownloadFile(downloadsDir: File, requestedFileName: String?): File {
    val fileName = sanitizeWebUIDownloadFileName(requestedFileName)
    return File(downloadsDir, fileName)
}

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
    target: File,
    source: InputStream,
    outputStreamFactory: (File) -> OutputStream = ::FileOutputStream,
    onBytesWritten: (Long) -> Unit = {},
): Long {
    target.parentFile?.mkdirs()
    outputStreamFactory(target).buffered(64 * 1024).use { output ->
        val buffer = ByteArray(8 * 1024)
        var total = 0L
        while (true) {
            val read = source.read(buffer)
            if (read == -1) {
                break
            }
            output.write(buffer, 0, read)
            total += read
            onBytesWritten(total)
        }
        output.flush()
        return total
    }
}
