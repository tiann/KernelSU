package me.weishu.kernelsu.ui.webui.file

import android.webkit.JavascriptInterface
import java.io.OutputStream
import java.util.concurrent.ConcurrentHashMap

const val TAG = "KsuIO"

object KsuIO {
    internal val openInputStreams = ConcurrentHashMap<String, ManagedInputStream>()
    internal val openOutputStreams = ConcurrentHashMap<String, OutputStream>()

    private val fileOutputStream = FileOutputStreamInterface()
    private val fileInputStream = FileInputStreamInterface()
    private val randomAccessFile = RandomAccessFileInterface()

    @JavascriptInterface
    fun File(path: String): FileInterface = FileInterface(path)

    @JavascriptInterface
    fun FileInputStream(): FileInputStreamInterface = fileInputStream

    @JavascriptInterface
    fun FileOutputStream(): FileOutputStreamInterface = fileOutputStream

    @JavascriptInterface
    fun RandomAccessFile(): RandomAccessFileInterface = randomAccessFile

    fun destroy() {
        fileInputStream.closeAll()
        fileOutputStream.closeAll()
        randomAccessFile.closeAll()
    }
}
