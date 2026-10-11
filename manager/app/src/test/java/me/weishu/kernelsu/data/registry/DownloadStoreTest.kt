package me.weishu.kernelsu.data.registry

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.ByteArrayInputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest

class DownloadStoreTest {

    @get:Rule
    val tmp = TemporaryFolder()

    private val data = ByteArray(50_000) { (it * 31 % 253).toByte() }
    private val sha = MessageDigest.getInstance("SHA-256").digest(data).joinToString("") { "%02x".format(it) }
    private val expected = ExpectedFile(data.size.toLong(), sha)

    private fun root() = tmp.newFolder("registry_downloads")

    /** Everything under [dir], recursively, as relative paths. */
    private fun listing(dir: File): List<String> =
        dir.walkTopDown().filter { it.isFile }.map { it.relativeTo(dir).path }.sorted().toList()

    @Test
    fun verifiedDownloadIsStoredUnderItsExpectation() {
        val root = root()
        var progress = 0L
        val file = RegistryDownloadStore.download(ByteArrayInputStream(data), root, expected, "My Module (v1).zip", { progress = it })
        assertEquals(data.size.toLong(), progress)
        assertArrayEquals(data, file.readBytes())
        assertEquals("My_Module__v1_.zip", file.name)
        assertEquals(RegistryDownloadStore.dirNameFor(expected), file.parentFile!!.name)
        assertEquals(expected, RegistryDownloadStore.expectedFor(root, file))
        assertEquals(listOf("${RegistryDownloadStore.dirNameFor(expected)}/My_Module__v1_.zip"), listing(root))
    }

    @Test
    fun hashMismatchDeletesEverything() {
        val root = root()
        val tampered = data.copyOf().also { it[100] = (it[100] + 1).toByte() }
        val e = assertThrows(DownloadVerificationException::class.java) {
            RegistryDownloadStore.download(ByteArrayInputStream(tampered), root, expected, "m.zip")
        }
        assertEquals(DownloadVerificationException.Reason.SHA256_MISMATCH, e.reason)
        assertEquals(emptyList<String>(), listing(root))
        assertEquals(0, root.list()!!.size)
    }

    @Test
    fun oversizedAndTruncatedDownloadsAreDeleted() {
        val root = root()
        val big = assertThrows(DownloadVerificationException::class.java) {
            RegistryDownloadStore.download(ByteArrayInputStream(data + byteArrayOf(1)), root, expected, "m.zip")
        }
        assertEquals(DownloadVerificationException.Reason.SIZE_MISMATCH, big.reason)
        val small = assertThrows(DownloadVerificationException::class.java) {
            RegistryDownloadStore.download(ByteArrayInputStream(data.copyOf(10)), root, expected, "m.zip")
        }
        assertEquals(DownloadVerificationException.Reason.SIZE_MISMATCH, small.reason)
        assertEquals(emptyList<String>(), listing(root))
    }

    @Test
    fun networkErrorAndCancellationDeletePartialFile() {
        val root = root()
        val failing = object : InputStream() {
            var n = 0
            override fun read(): Int = throw IOException("reset")
            override fun read(b: ByteArray, off: Int, len: Int): Int {
                if (n++ > 2) throw IOException("connection reset")
                b[off] = data[n]
                return 1
            }
        }
        assertThrows(IOException::class.java) { RegistryDownloadStore.download(failing, root, expected, "m.zip") }
        var calls = 0
        assertThrows(IllegalStateException::class.java) {
            RegistryDownloadStore.download(ByteArrayInputStream(data), root, expected, "m.zip", checkActive = {
                if (++calls > 2) throw IllegalStateException("cancelled")
            })
        }
        assertEquals(emptyList<String>(), listing(root))
    }

    @Test
    fun reHashBeforeFlashAcceptsExactBytes() {
        val stored = RegistryDownloadStore.download(ByteArrayInputStream(data), root(), expected, "m.zip")
        val dest = tmp.newFile("install.zip")
        stored.inputStream().use { copyForInstall(it, dest, expected) }
        assertArrayEquals(data, dest.readBytes())
    }

    @Test
    fun reHashBeforeFlashRejectsModifiedFileAndDeletesCopy() {
        val root = root()
        val stored = RegistryDownloadStore.download(ByteArrayInputStream(data), root, expected, "m.zip")
        // Same size, different content: modified after the download was verified.
        stored.writeBytes(data.copyOf().also { it[0] = (it[0] + 1).toByte() })
        val dest = File(tmp.root, "install.zip")
        val e = assertThrows(DownloadVerificationException::class.java) {
            stored.inputStream().use { copyForInstall(it, dest, RegistryDownloadStore.expectedFor(root, stored)) }
        }
        assertEquals(DownloadVerificationException.Reason.SHA256_MISMATCH, e.reason)
        assertFalse(dest.exists())

        stored.writeBytes(data + byteArrayOf(0))
        val sizeError = assertThrows(DownloadVerificationException::class.java) { verifyFile(stored, expected) }
        assertEquals(DownloadVerificationException.Reason.SIZE_MISMATCH, sizeError.reason)
        assertTrue(RegistryDownloadStore.delete(root, stored))
        assertEquals(emptyList<String>(), listing(root))
    }

    @Test
    fun unverifiedInstallJustCopies() {
        val dest = File(tmp.root, "plain.zip")
        copyForInstall(ByteArrayInputStream(byteArrayOf(1, 2, 3)), dest, null)
        assertArrayEquals(byteArrayOf(1, 2, 3), dest.readBytes())
    }

    @Test
    fun onlyFilesInsideTheStoreCarryAnExpectation() {
        val root = root()
        val stored = RegistryDownloadStore.download(ByteArrayInputStream(data), root, expected, "m.zip")
        val outside = File(tmp.newFolder(RegistryDownloadStore.dirNameFor(expected)), "m.zip").apply { writeBytes(data) }
        assertNull(RegistryDownloadStore.expectedFor(root, outside))
        assertNull(RegistryDownloadStore.expectedFor(root, File(root, "m.zip")))
        assertNull(RegistryDownloadStore.expectedFor(root, File(File(root, "not-a-hash"), "m.zip")))
        // Path tricks resolve to the canonical location first.
        val dotted = File(root, "${stored.parentFile!!.name}/../${stored.parentFile!!.name}/m.zip")
        assertEquals(expected, RegistryDownloadStore.expectedFor(root, dotted))
        assertNull(RegistryDownloadStore.expectedFor(root, File(root, "${stored.parentFile!!.name}/../../x/m.zip")))
        assertFalse(RegistryDownloadStore.delete(root, outside))
        assertTrue(outside.exists())
    }

    @Test
    fun dirNamesRoundTrip() {
        assertEquals(expected, RegistryDownloadStore.expectedFromDirName(RegistryDownloadStore.dirNameFor(expected)))
        assertEquals(
            RegistryDownloadStore.dirNameFor(expected),
            RegistryDownloadStore.dirNameFor(ExpectedFile(expected.size, sha.uppercase()))
        )
        for (bad in listOf("", "x", "$sha-0", "$sha-", "${sha.uppercase()}-1", "$sha-1-2", "${sha.dropLast(1)}-1", "$sha-99999999999999999999")) {
            assertNull(bad, RegistryDownloadStore.expectedFromDirName(bad))
        }
        assertEquals("module.zip", RegistryDownloadStore.sanitizeFileName("../.."))
        assertEquals("evil.zip", RegistryDownloadStore.sanitizeFileName("a/b/../evil.zip"))
    }

    @Test
    fun cleanupRemovesOldEntriesAndStrayFiles() {
        val root = root()
        val stored = RegistryDownloadStore.download(ByteArrayInputStream(data), root, expected, "m.zip")
        File(root, "stray.txt").writeText("x")
        RegistryDownloadStore.cleanup(root, maxAgeMillis = 60_000, now = stored.lastModified() + 1_000)
        assertEquals(listOf("${stored.parentFile!!.name}/m.zip", "stray.txt"), listing(root))
        RegistryDownloadStore.cleanup(root, maxAgeMillis = 60_000, now = stored.lastModified() + 120_000)
        assertEquals(0, root.list()!!.size)
    }

    @Test
    fun cleanupKeepsFreshDirectoriesOfConcurrentDownloads() {
        val root = root()
        // Just created by a concurrent download, before (or while) it writes its .part file.
        val empty = File(root, RegistryDownloadStore.dirNameFor(expected)).apply { mkdirs() }
        val other = ExpectedFile(expected.size, "b".repeat(64))
        val partial = File(root, RegistryDownloadStore.dirNameFor(other)).apply { mkdirs() }
        val part = File.createTempFile("dl-", ".part", partial)
        val now = maxOf(empty.lastModified(), partial.lastModified(), part.lastModified())

        RegistryDownloadStore.cleanup(root, maxAgeMillis = 60_000, now = now + 1_000)
        assertTrue(empty.isDirectory)
        assertTrue(part.isFile)

        RegistryDownloadStore.cleanup(root, maxAgeMillis = 60_000, now = now + 120_000)
        assertEquals(0, root.list()!!.size)
    }

    @Test
    fun verifiedAndUnverifiedDownloadsAreNeverShared() {
        val url = "https://github.com/o/r/releases/download/v1/m.zip"
        assertTrue(canShareDownload(url, null, url, null))
        assertTrue(canShareDownload(url, expected, url, ExpectedFile(expected.size, sha.uppercase())))
        assertFalse(canShareDownload(url, expected, url, null))
        assertFalse(canShareDownload(url, null, url, expected))
        assertFalse(canShareDownload(url, expected, url, ExpectedFile(expected.size + 1, sha)))
        assertFalse(canShareDownload(url, expected, url, ExpectedFile(expected.size, "b".repeat(64))))
        assertFalse(canShareDownload(url, expected, "$url?x", expected))
    }
}
