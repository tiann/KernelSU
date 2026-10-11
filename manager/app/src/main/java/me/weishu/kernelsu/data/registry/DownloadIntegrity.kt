package me.weishu.kernelsu.data.registry

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest

/** Expected properties of a registry release ZIP. */
data class ExpectedFile(val size: Long, val sha256: String) {
    /** Same expectation regardless of hash spelling. */
    fun sameAs(other: ExpectedFile?): Boolean =
        other != null && size == other.size && sha256.equals(other.sha256, ignoreCase = true)
}

class DownloadVerificationException(val reason: Reason, message: String) : IOException(message) {
    enum class Reason { URL_NOT_ALLOWED, SIZE_MISMATCH, SHA256_MISMATCH }
}

/**
 * Two requests may share one download only when they ask for the same URL *and* the same
 * verification: a verified (registry) request must never reuse an unverified download, and
 * vice versa.
 */
fun canShareDownload(url: String, expected: ExpectedFile?, otherUrl: String, otherExpected: ExpectedFile?): Boolean {
    if (url != otherUrl) return false
    return if (expected == null) otherExpected == null else expected.sameAs(otherExpected)
}

/**
 * Streaming size + SHA-256 check. Feed every chunk to [update] (aborts early once more bytes
 * than expected arrive) and call [finish] before the file is handed to the installer.
 */
class DownloadVerifier(private val expected: ExpectedFile) {
    private val digest = MessageDigest.getInstance("SHA-256")
    private val expectedHash = expected.sha256.lowercase()
    var bytesSeen: Long = 0L
        private set

    init {
        require(expected.size > 0) { "expected size must be positive" }
        require(expectedHash.length == 64 && expectedHash.all { it in '0'..'9' || it in 'a'..'f' }) {
            "expected sha256 must be 64 hex chars"
        }
    }

    @Throws(DownloadVerificationException::class)
    fun update(buffer: ByteArray, offset: Int = 0, length: Int = buffer.size) {
        bytesSeen += length
        if (bytesSeen > expected.size) {
            throw DownloadVerificationException(
                DownloadVerificationException.Reason.SIZE_MISMATCH,
                "Downloaded more than the expected ${expected.size} bytes"
            )
        }
        digest.update(buffer, offset, length)
    }

    @Throws(DownloadVerificationException::class)
    fun finish() {
        if (bytesSeen != expected.size) {
            throw DownloadVerificationException(
                DownloadVerificationException.Reason.SIZE_MISMATCH,
                "Size mismatch: expected ${expected.size}, got $bytesSeen"
            )
        }
        val actual = digest.digest().joinToString("") { "%02x".format(it) }
        if (!MessageDigest.isEqual(actual.toByteArray(), expectedHash.toByteArray())) {
            throw DownloadVerificationException(
                DownloadVerificationException.Reason.SHA256_MISMATCH,
                "SHA-256 mismatch: expected $expectedHash, got $actual"
            )
        }
    }
}

/** Re-hashes [file] from disk; throws [DownloadVerificationException] on any mismatch. */
@Throws(IOException::class)
fun verifyFile(file: File, expected: ExpectedFile) {
    val verifier = DownloadVerifier(expected)
    if (file.length() != expected.size) {
        throw DownloadVerificationException(
            DownloadVerificationException.Reason.SIZE_MISMATCH,
            "Size mismatch: expected ${expected.size}, got ${file.length()}"
        )
    }
    file.inputStream().use { input ->
        val buf = ByteArray(64 * 1024)
        while (true) {
            val read = input.read(buf)
            if (read == -1) break
            verifier.update(buf, 0, read)
        }
    }
    verifier.finish()
}

/**
 * Copies a module ZIP to [dest] (the exact file handed to `ksud module install`) and, when
 * [expected] is set, re-hashes the copy on disk right before installing. On mismatch [dest]
 * is deleted and [DownloadVerificationException] is thrown.
 */
@Throws(IOException::class)
fun copyForInstall(input: InputStream, dest: File, expected: ExpectedFile?) {
    try {
        FileOutputStream(dest).use { out ->
            input.copyTo(out)
            out.flush()
            out.fd.sync()
        }
        if (expected != null) verifyFile(dest, expected)
    } catch (e: Throwable) {
        dest.delete()
        throw e
    }
}

/**
 * App-private store for registry ZIPs: `{root}/{sha256}-{size}/{fileName}`. Files never leave
 * app-private storage, and the directory name keeps the expected size + SHA-256 attached to
 * the file all the way to the flash step, which re-verifies the bytes it installs.
 */
object RegistryDownloadStore {
    const val DIR_NAME = "registry_downloads"

    private val DIR_PATTERN = Regex("^([0-9a-f]{64})-([1-9][0-9]{0,17})$")
    private val UNSAFE_NAME_CHARS = Regex("[^A-Za-z0-9._-]")

    fun dirNameFor(expected: ExpectedFile): String = "${expected.sha256.lowercase()}-${expected.size}"

    fun expectedFromDirName(name: String): ExpectedFile? {
        val m = DIR_PATTERN.matchEntire(name) ?: return null
        val size = m.groupValues[2].toLongOrNull() ?: return null
        return ExpectedFile(size, m.groupValues[1])
    }

    fun sanitizeFileName(name: String): String {
        val base = name.substringAfterLast('/').substringAfterLast('\\')
            .replace(UNSAFE_NAME_CHARS, "_")
            .trimStart('.')
            .take(120)
        return base.ifEmpty { "module.zip" }
    }

    /** Expected size/hash of [file] if it lives in the store under [root], otherwise null. */
    fun expectedFor(root: File, file: File): ExpectedFile? {
        val canonical = runCatching { file.canonicalFile }.getOrNull() ?: return null
        val parent = canonical.parentFile ?: return null
        val storeRoot = runCatching { root.canonicalFile }.getOrNull() ?: return null
        if (parent.parentFile != storeRoot) return null
        return expectedFromDirName(parent.name)
    }

    /**
     * Streams [input] into a unique temporary file, verifying size + SHA-256 on the fly, fsyncs
     * it and only then moves it to its final name. Any failure (including a verification
     * mismatch) deletes the partial file; nothing unverified is ever left under its final name.
     */
    @Throws(IOException::class)
    fun download(
        input: InputStream,
        root: File,
        expected: ExpectedFile,
        fileName: String,
        onProgress: (bytes: Long) -> Unit = {},
        checkActive: () -> Unit = {},
    ): File {
        val verifier = DownloadVerifier(expected)
        val dir = File(root, dirNameFor(expected))
        if (!dir.isDirectory && !dir.mkdirs()) throw IOException("Cannot create $dir")
        val target = File(dir, sanitizeFileName(fileName))
        val part = File.createTempFile("dl-", ".part", dir)
        try {
            FileOutputStream(part).use { out ->
                val buf = ByteArray(8 * 1024)
                while (true) {
                    checkActive()
                    val read = input.read(buf)
                    if (read == -1) break
                    verifier.update(buf, 0, read)
                    out.write(buf, 0, read)
                    onProgress(verifier.bytesSeen)
                }
                out.flush()
                out.fd.sync()
            }
            verifier.finish()
            if (target.exists() && !target.delete()) throw IOException("Cannot replace $target")
            if (!part.renameTo(target)) throw IOException("Cannot move download to $target")
            return target
        } catch (e: Throwable) {
            part.delete()
            if (dir.list()?.isEmpty() == true) dir.delete()
            throw e
        }
    }

    /** Deletes a stored file (and its directory when empty); false if [file] is not in the store. */
    fun delete(root: File, file: File): Boolean {
        if (expectedFor(root, file) == null) return false
        val canonical = file.canonicalFile
        val deleted = canonical.delete()
        val dir = canonical.parentFile
        if (dir?.list()?.isEmpty() == true) dir.delete()
        return deleted
    }

    /**
     * Removes entries older than [maxAgeMillis] (and old leftovers that are not store entries).
     * Recent directories are kept even when empty: a concurrent download may have just created
     * its directory and not yet its temporary file.
     */
    fun cleanup(root: File, maxAgeMillis: Long, now: Long = System.currentTimeMillis()) {
        fun isOld(f: File) = now - f.lastModified() > maxAgeMillis
        root.listFiles()?.forEach { dir ->
            // Checked before deleting anything inside, which bumps the directory's mtime.
            val dirOld = isOld(dir)
            if (!dir.isDirectory || expectedFromDirName(dir.name) == null) {
                if (dirOld) dir.deleteRecursively()
                return@forEach
            }
            dir.listFiles()?.forEach { f -> if (isOld(f)) f.delete() }
            if (dirOld && dir.list()?.isEmpty() == true) dir.delete()
        }
    }
}
