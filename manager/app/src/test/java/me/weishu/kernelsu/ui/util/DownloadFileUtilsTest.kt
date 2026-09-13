package me.weishu.kernelsu.ui.util

import java.nio.file.Files
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DownloadFileUtilsTest {
    @Test
    fun sanitizeDownloadFileName_reducesInputToOneSafeComponent() {
        assertEquals("evil.zip", sanitizeDownloadFileName("../../evil.zip"))
        assertEquals("evil.zip", sanitizeDownloadFileName("..\\..\\evil.zip"))
        assertEquals("evil.zip", sanitizeDownloadFileName("  evil\u0000.zip  "))
        assertEquals("download.bin", sanitizeDownloadFileName(""))
        assertEquals("download.bin", sanitizeDownloadFileName("."))
        assertEquals("download.bin", sanitizeDownloadFileName(".."))
    }

    @Test
    fun resolveAvailableDownloadTarget_addsCollisionSuffixBeforeExtension() {
        val directory = Files.createTempDirectory("kernelsu-download").toFile()
        try {
            directory.resolve("module.zip").createNewFile()
            directory.resolve("module (1).zip").createNewFile()

            val reserved = reserveAvailableDownloadTarget(directory, "../../module.zip")
            reserved.output.close()

            assertEquals("module (2).zip", reserved.file.name)
            assertTrue(reserved.file.parentFile.canonicalFile == directory.canonicalFile)
        } finally {
            directory.deleteRecursively()
        }
    }

    @Test
    fun resolveAvailableDownloadTarget_skipsSymlinkOutsideDirectory() {
        val root = Files.createTempDirectory("kernelsu-download").toFile()
        val directory = root.resolve("downloads").apply { mkdirs() }
        val outside = root.resolve("outside.zip").apply { createNewFile() }
        try {
            Files.createSymbolicLink(directory.resolve("module.zip").toPath(), outside.toPath())

            val reserved = reserveAvailableDownloadTarget(directory, "module.zip")
            reserved.output.close()

            assertEquals("module (1).zip", reserved.file.name)
            assertTrue(reserved.file.exists())
            assertTrue(reserved.file.parentFile.canonicalFile == directory.canonicalFile)
        } finally {
            root.deleteRecursively()
        }
    }
}
