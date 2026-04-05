package me.weishu.kernelsu.ui.webui.file

import android.util.Base64
import android.util.Log
import android.webkit.JavascriptInterface
import com.topjohnwu.superuser.io.SuFileInputStream
import java.io.BufferedInputStream
import java.io.InputStream

class FileInputStreamInterface {
    @JavascriptInterface
    fun open(path: String): String = runCatching {
        val stream = ManagedInputStream(SuFileInputStream.open(path))
        val id = java.util.UUID.randomUUID().toString()
        KsuIO.openInputStreams[id] = stream
        id
    }.onFailure { Log.e(TAG, "FileInputStream open failed", it) }.getOrElse { "" }

    @JavascriptInterface
    fun read(id: String): String = runCatching {
        val stream = KsuIO.openInputStreams[id] ?: return ""
        val bytesRead = stream.readInto()
        if (bytesRead > 0) {
            Base64.encodeToString(stream.buffer(), 0, bytesRead, Base64.NO_WRAP)
        } else {
            ""
        }
    }.onFailure { Log.e(TAG, "FileInputStream read failed", it) }.getOrElse { "" }

    @JavascriptInterface
    fun read(id: String, maxBytes: Int): String = runCatching {
        val stream = KsuIO.openInputStreams[id] ?: return ""
        val bytesRead = stream.readInto(maxBytes)
        if (bytesRead > 0) {
            Base64.encodeToString(stream.buffer(), 0, bytesRead, Base64.NO_WRAP)
        } else {
            ""
        }
    }.onFailure { Log.e(TAG, "FileInputStream read(maxBytes) failed", it) }.getOrElse { "" }

    @JavascriptInterface
    fun available(id: String): Int = runCatching {
        val stream = KsuIO.openInputStreams[id] ?: return 0
        stream.available()
    }.getOrDefault(0)

    @JavascriptInterface
    fun close(id: String): Boolean {
        val stream = KsuIO.openInputStreams.remove(id) ?: return false
        return runCatching {
            stream.close()
            true
        }.getOrElse {
            Log.e(TAG, "FileInputStream close failed", it)
            false
        }
    }

    fun closeAll() {
        while (true) {
            val entry = KsuIO.openInputStreams.entries.firstOrNull() ?: break
            val id = entry.key
            val stream = entry.value
            if (!KsuIO.openInputStreams.remove(id, stream)) {
                continue
            }
            runCatching {
                stream.close()
            }.onFailure { Log.e(TAG, "closeAll failed for $id", it) }
        }
    }
}

internal class ManagedInputStream(
    inputStream: InputStream,
    private val defaultChunkSize: Int = DEFAULT_CHUNK_SIZE,
    private val maxChunkSize: Int = MAX_CHUNK_SIZE,
) : AutoCloseable {
    private val stream = BufferedInputStream(inputStream, maxChunkSize)
    private var buffer = ByteArray(defaultChunkSize)

    @Synchronized
    fun readInto(maxBytes: Int = defaultChunkSize): Int {
        val requestedSize = maxBytes.coerceIn(1, maxChunkSize)
        if (buffer.size < requestedSize) {
            buffer = ByteArray(requestedSize)
        }
        return stream.read(buffer, 0, requestedSize)
    }

    @Synchronized
    fun buffer(): ByteArray = buffer

    @Synchronized
    fun available(): Int = stream.available()

    @Synchronized
    override fun close() {
        stream.close()
    }

    companion object {
        const val DEFAULT_CHUNK_SIZE = 8 * 1024
        const val MAX_CHUNK_SIZE = 64 * 1024
    }
}
