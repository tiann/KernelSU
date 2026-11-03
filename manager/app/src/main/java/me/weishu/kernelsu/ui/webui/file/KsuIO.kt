package me.weishu.kernelsu.ui.webui.file

import android.webkit.JavascriptInterface
import java.io.OutputStream
import java.util.concurrent.ConcurrentHashMap

const val TAG = "KsuIO"

/**
 * The `ksu.io` object, one per WebUI window.
 *
 * This used to be a process-wide `object`, which made every open stream a resource of the whole
 * app. Two module pages can be open at once - `WebUIActivity` is `documentLaunchMode="intoExisting"`
 * with its own `taskAffinity` - and closing either one called `closeAll()` on the other's handles.
 * The page that lost its streams then saw `read()` answer "" and `close()` answer false, with no
 * way to tell that from a file it never opened.
 *
 * The JavaScript contract is unchanged: `ksu.io()` still returns an object with the same members,
 * because every call is resolved on the returned object rather than on a global.
 *
 * The members reached from JavaScript are never referenced from Kotlin, so the inspection that
 * reports them as unused is describing the calling convention, not a defect. The PascalCase names
 * are part of the same contract - the page calls `ksu.io().File(path)`.
 */
@Suppress("unused", "FunctionName")
class KsuIO {
    private val openInputStreams = ConcurrentHashMap<String, ManagedInputStream>()
    private val openOutputStreams = ConcurrentHashMap<String, OutputStream>()

    private val fileOutputStream = FileOutputStreamInterface(this)
    private val fileInputStream = FileInputStreamInterface(this)
    private val randomAccessFile = RandomAccessFileInterface()

    @JavascriptInterface
    fun version(): Int = KSU_IO_VERSION

    @JavascriptInterface
    fun File(path: String): FileInterface = FileInterface(path, this)

    @JavascriptInterface
    fun FileInputStream(): FileInputStreamInterface = fileInputStream

    @JavascriptInterface
    fun FileOutputStream(): FileOutputStreamInterface = fileOutputStream

    @JavascriptInterface
    fun RandomAccessFile(): RandomAccessFileInterface = randomAccessFile

    internal fun registerInput(stream: ManagedInputStream): String {
        val id = java.util.UUID.randomUUID().toString()
        openInputStreams[id] = stream
        return id
    }

    internal fun registerOutput(stream: OutputStream): String {
        val id = java.util.UUID.randomUUID().toString()
        openOutputStreams[id] = stream
        return id
    }

    internal fun input(id: String): ManagedInputStream? = openInputStreams[id]

    internal fun output(id: String): OutputStream? = openOutputStreams[id]

    internal fun removeInput(id: String): ManagedInputStream? = openInputStreams.remove(id)

    internal fun removeOutput(id: String): OutputStream? = openOutputStreams.remove(id)

    internal fun pruneInputs(close: (ManagedInputStream) -> Unit) {
        while (true) {
            val entry = openInputStreams.entries.firstOrNull() ?: break
            if (!openInputStreams.remove(entry.key, entry.value)) continue
            close(entry.value)
        }
    }

    internal fun pruneOutputs(close: (OutputStream) -> Unit) {
        while (true) {
            val entry = openOutputStreams.entries.firstOrNull() ?: break
            if (!openOutputStreams.remove(entry.key, entry.value)) continue
            close(entry.value)
        }
    }

    /** Closes every stream this window opened. Another window's streams are not affected. */
    fun destroy() {
        fileInputStream.closeAll()
        fileOutputStream.closeAll()
        randomAccessFile.closeAll()
    }

    /**
     * Check if we can open a new stream without exceeding resource limits.
     * Returns true if under limit, false otherwise.
     */
    internal fun canOpenStream(): Boolean {
        return openInputStreams.size + openOutputStreams.size < MAX_OPEN_STREAMS
    }

    companion object {
        private const val KSU_IO_VERSION = 1
        private const val MAX_OPEN_STREAMS = 50
    }
}
