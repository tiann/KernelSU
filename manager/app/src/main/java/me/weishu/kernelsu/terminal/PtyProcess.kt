package me.weishu.kernelsu.terminal

import androidx.annotation.Keep
import java.io.Closeable

/** A child process whose standard streams share a controlling terminal. */
@Keep
class PtyProcess(command: Array<String>, columns: Int, rows: Int) : Closeable {
    private val child = nativeStart(command.map { it.toByteArray(Charsets.UTF_8) }.toTypedArray(), columns, rows)
    private var fd = child[0]
    private var exitCode: Int? = null

    @Synchronized
    fun read(buffer: ByteArray): Int = if (fd < 0) -1 else nativeRead(fd, buffer)

    @Synchronized
    fun resize(columns: Int, rows: Int) {
        if (fd >= 0) nativeResize(fd, columns, rows)
    }

    fun pollExit(): Int? {
        if (exitCode == null) exitCode = nativeWait(child[1], true).takeIf { it >= 0 }
        return exitCode
    }

    fun waitFor(): Int = exitCode ?: nativeWait(child[1], false).also { exitCode = it }

    @Synchronized
    override fun close() {
        if (fd >= 0) {
            // Closing the master also hangs up the child's controlling terminal.
            nativeClose(fd)
            fd = -1
        }
    }

    private external fun nativeStart(command: Array<ByteArray>, columns: Int, rows: Int): IntArray
    private external fun nativeRead(fd: Int, buffer: ByteArray): Int
    private external fun nativeResize(fd: Int, columns: Int, rows: Int)
    private external fun nativeClose(fd: Int)
    private external fun nativeWait(pid: Int, noHang: Boolean): Int

    companion object {
        init {
            System.loadLibrary("kernelsu")
        }
    }
}
