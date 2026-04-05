package me.weishu.kernelsu.ui.webui.file

import android.util.Base64
import android.util.Log
import android.webkit.JavascriptInterface
import com.topjohnwu.superuser.io.SuRandomAccessFile
import java.util.concurrent.ConcurrentHashMap

class RandomAccessFileInterface {
    private val openFiles = ConcurrentHashMap<String, SuRandomAccessFile>()

    @JavascriptInterface
    fun open(path: String, mode: String): String = runCatching {
        val raf = SuRandomAccessFile.open(path, mode)
        val id = java.util.UUID.randomUUID().toString()
        openFiles[id] = raf
        id
    }.onFailure { Log.e(TAG, "RandomAccessFile open failed", it) }.getOrElse { "" }

    @JavascriptInterface
    fun read(id: String): Int = runCatching {
        val raf = openFiles[id] ?: return -1
        synchronized(raf) { raf.read() }
    }.onFailure { Log.e(TAG, "RandomAccessFile read() failed", it) }.getOrElse { -1 }

    @JavascriptInterface
    fun readBytes(id: String, len: Int): String = runCatching {
        val raf = openFiles[id] ?: return ""
        val buffer = ByteArray(len)
        val bytesRead = synchronized(raf) { raf.read(buffer) }
        if (bytesRead > 0) {
            Base64.encodeToString(buffer, 0, bytesRead, Base64.NO_WRAP)
        } else {
            ""
        }
    }.onFailure { Log.e(TAG, "RandomAccessFile readBytes failed", it) }.getOrElse { "" }

    @JavascriptInterface
    fun readBoolean(id: String): Boolean = runCatching {
        val raf = openFiles[id] ?: return false
        synchronized(raf) { raf.readBoolean() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readBoolean failed", it) }.getOrElse { false }

    @JavascriptInterface
    fun readByte(id: String): Byte = runCatching {
        val raf = openFiles[id] ?: return 0
        synchronized(raf) { raf.readByte() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readByte failed", it) }.getOrElse { 0 }

    @JavascriptInterface
    fun readInt(id: String): Int = runCatching {
        val raf = openFiles[id] ?: return 0
        synchronized(raf) { raf.readInt() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readInt failed", it) }.getOrElse { 0 }

    @JavascriptInterface
    fun readLong(id: String): Long = runCatching {
        val raf = openFiles[id] ?: return 0L
        synchronized(raf) { raf.readLong() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readLong failed", it) }.getOrElse { 0L }

    @JavascriptInterface
    fun readShort(id: String): Short = runCatching {
        val raf = openFiles[id] ?: return 0.toShort()
        synchronized(raf) { raf.readShort() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readShort failed", it) }.getOrElse { 0.toShort() }

    @JavascriptInterface
    fun readFloat(id: String): Float = runCatching {
        val raf = openFiles[id] ?: return 0f
        synchronized(raf) { raf.readFloat() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readFloat failed", it) }.getOrElse { 0f }

    @JavascriptInterface
    fun readDouble(id: String): Double = runCatching {
        val raf = openFiles[id] ?: return 0.0
        synchronized(raf) { raf.readDouble() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readDouble failed", it) }.getOrElse { 0.0 }

    @JavascriptInterface
    fun readUTF(id: String): String = runCatching {
        val raf = openFiles[id] ?: return ""
        synchronized(raf) { raf.readUTF() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readUTF failed", it) }.getOrElse { "" }

    @JavascriptInterface
    fun readLine(id: String): String? = runCatching {
        val raf = openFiles[id] ?: return null
        synchronized(raf) { raf.readLine() }
    }.onFailure { Log.e(TAG, "RandomAccessFile readLine failed", it) }.getOrNull()

    @JavascriptInterface
    fun write(id: String, b: Int): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.write(b) }
    }.onFailure { Log.e(TAG, "RandomAccessFile write failed", it) }.let { }

    @JavascriptInterface
    fun writeBase64(id: String, data: String): Unit = runCatching {
        val raf = openFiles[id] ?: return
        val bytes = Base64.decode(data, Base64.NO_WRAP)
        synchronized(raf) { raf.write(bytes) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeBase64 failed", it) }.let { }

    @JavascriptInterface
    fun writeBoolean(id: String, v: Boolean): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeBoolean(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeBoolean failed", it) }.let { }

    @JavascriptInterface
    fun writeByte(id: String, v: Int): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeByte(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeByte failed", it) }.let { }

    @JavascriptInterface
    fun writeInt(id: String, v: Int): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeInt(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeInt failed", it) }.let { }

    @JavascriptInterface
    fun writeLong(id: String, v: Long): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeLong(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeLong failed", it) }.let { }

    @JavascriptInterface
    fun writeShort(id: String, v: Int): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeShort(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeShort failed", it) }.let { }

    @JavascriptInterface
    fun writeFloat(id: String, v: Float): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeFloat(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeFloat failed", it) }.let { }

    @JavascriptInterface
    fun writeDouble(id: String, v: Double): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeDouble(v) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeDouble failed", it) }.let { }

    @JavascriptInterface
    fun writeUTF(id: String, str: String): Unit = runCatching {
        val raf = openFiles[id] ?: return
        synchronized(raf) { raf.writeUTF(str) }
    }.onFailure { Log.e(TAG, "RandomAccessFile writeUTF failed", it) }.let { }

    @JavascriptInterface
    fun seek(id: String, pos: Long): Boolean = runCatching {
        val raf = openFiles[id] ?: return false
        synchronized(raf) { raf.seek(pos) }
        true
    }.onFailure { Log.e(TAG, "RandomAccessFile seek failed", it) }.getOrElse { false }

    @JavascriptInterface
    fun getFilePointer(id: String): Long = runCatching {
        val raf = openFiles[id] ?: return -1L
        synchronized(raf) { raf.getFilePointer() }
    }.onFailure { Log.e(TAG, "RandomAccessFile getFilePointer failed", it) }.getOrElse { -1L }

    @JavascriptInterface
    fun length(id: String): Long = runCatching {
        val raf = openFiles[id] ?: return -1L
        synchronized(raf) { raf.length() }
    }.onFailure { Log.e(TAG, "RandomAccessFile length failed", it) }.getOrElse { -1L }

    @JavascriptInterface
    fun setLength(id: String, newLength: Long): Boolean = runCatching {
        val raf = openFiles[id] ?: return false
        synchronized(raf) { raf.setLength(newLength) }
        true
    }.onFailure { Log.e(TAG, "RandomAccessFile setLength failed", it) }.getOrElse { false }

    @JavascriptInterface
    fun close(id: String): Boolean {
        val raf = openFiles.remove(id) ?: return false
        return runCatching {
            synchronized(raf) { raf.close() }
            true
        }.onFailure { Log.e(TAG, "RandomAccessFile close failed", it) }.getOrElse { false }
    }

    fun closeAll() {
        openFiles.forEach { (id, raf) ->
            runCatching {
                synchronized(raf) { raf.close() }
            }.onFailure { Log.e(TAG, "closeAll failed for $id", it) }
        }
        openFiles.clear()
    }
}
