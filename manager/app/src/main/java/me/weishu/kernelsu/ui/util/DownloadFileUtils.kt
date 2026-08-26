package me.weishu.kernelsu.ui.util

import java.io.File
import java.io.OutputStream
import java.nio.file.FileAlreadyExistsException
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardOpenOption

private const val FALLBACK_FILE_NAME = "download.bin"

/**
 * Reduces an untrusted name (module metadata, release asset name) to a single
 * path component so it cannot escape the target directory.
 */
internal fun sanitizeDownloadFileName(fileName: String): String {
    val name = fileName.substringAfterLast('/').substringAfterLast('\\')
        .filterNot { it == '\u0000' }
        .trim()
    return if (name.isEmpty() || name == "." || name == "..") FALLBACK_FILE_NAME else name
}

internal data class ReservedDownloadTarget(
    val file: File,
    val output: OutputStream,
)

/**
 * Atomically reserves a new target inside [directory]. Canonical-path checks
 * reject traversal and symlink escapes, while CREATE_NEW prevents two
 * concurrent downloads from selecting the same filename.
 */
internal fun reserveAvailableDownloadTarget(
    directory: File,
    rawFileName: String,
): ReservedDownloadTarget {
    val canonicalDirectory = directory.canonicalFile
    val canonicalDirectoryPath: Path = canonicalDirectory.toPath()
    val fileName = sanitizeDownloadFileName(rawFileName)
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
        val candidate = File(canonicalDirectory, candidateName)
        val canonicalCandidate = candidate.canonicalFile
        if (!canonicalCandidate.toPath().startsWith(canonicalDirectoryPath)) {
            index++
            continue
        }
        try {
            val output = Files.newOutputStream(
                candidate.toPath(),
                StandardOpenOption.CREATE_NEW,
                StandardOpenOption.WRITE,
            )
            return ReservedDownloadTarget(candidate, output)
        } catch (_: FileAlreadyExistsException) {
            index++
        }
    }
}
