package me.weishu.kernelsu.data.registry

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.security.MessageDigest

class DownloadVerifierTest {

    private val data = ByteArray(10_000) { (it % 251).toByte() }
    private val sha = MessageDigest.getInstance("SHA-256").digest(data).joinToString("") { "%02x".format(it) }

    private fun feed(verifier: DownloadVerifier, bytes: ByteArray, chunk: Int = 4096) {
        var off = 0
        while (off < bytes.size) {
            val len = minOf(chunk, bytes.size - off)
            verifier.update(bytes, off, len)
            off += len
        }
    }

    @Test
    fun acceptsMatchingFile() {
        val v = DownloadVerifier(ExpectedFile(data.size.toLong(), sha))
        feed(v, data)
        v.finish()
        assertEquals(data.size.toLong(), v.bytesSeen)
        // Upper-case expected hash is accepted too.
        DownloadVerifier(ExpectedFile(data.size.toLong(), sha.uppercase())).also { feed(it, data); it.finish() }
    }

    @Test
    fun rejectsHashMismatch() {
        val tampered = data.copyOf().also { it[42] = (it[42] + 1).toByte() }
        val v = DownloadVerifier(ExpectedFile(data.size.toLong(), sha))
        feed(v, tampered)
        val e = assertThrows(DownloadVerificationException::class.java) { v.finish() }
        assertEquals(DownloadVerificationException.Reason.SHA256_MISMATCH, e.reason)
    }

    @Test
    fun rejectsShortFile() {
        val v = DownloadVerifier(ExpectedFile(data.size.toLong(), sha))
        feed(v, data.copyOf(data.size - 1))
        val e = assertThrows(DownloadVerificationException::class.java) { v.finish() }
        assertEquals(DownloadVerificationException.Reason.SIZE_MISMATCH, e.reason)
    }

    @Test
    fun abortsEarlyOnOversizedFile() {
        val v = DownloadVerifier(ExpectedFile(data.size.toLong(), sha))
        val e = assertThrows(DownloadVerificationException::class.java) { feed(v, data + byteArrayOf(0)) }
        assertEquals(DownloadVerificationException.Reason.SIZE_MISMATCH, e.reason)
    }

    @Test
    fun rejectsInvalidExpectation() {
        assertThrows(IllegalArgumentException::class.java) { DownloadVerifier(ExpectedFile(0, sha)) }
        assertThrows(IllegalArgumentException::class.java) { DownloadVerifier(ExpectedFile(1, "abc")) }
    }
}
