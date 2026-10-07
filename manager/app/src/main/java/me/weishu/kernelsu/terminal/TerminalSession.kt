package me.weishu.kernelsu.terminal

import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.io.ByteArrayOutputStream
import java.io.Closeable
import me.weishu.kernelsu.ui.util.terminalShellCommand

class TerminalSession : Closeable {
    var isReady by mutableStateOf(false)
        private set
    val emulator = TerminalEmulator(256, 24, 8, 16, null)
    private val mainHandler = Handler(Looper.getMainLooper())
    private val log = ByteArrayOutputStream()
    private val pending = ByteArrayOutputStream()
    private var updatePosted = false
    private var closed = false
    private var process: PtyProcess? = null
    private var columns = 256
    private var rows = 24

    @Synchronized
    fun append(bytes: ByteArray, count: Int = bytes.size) {
        log.write(bytes, 0, count)
        pending.write(bytes, 0, count)
        if (!updatePosted) {
            updatePosted = true
            mainHandler.post {
                val output = synchronized(this) {
                    updatePosted = false
                    pending.toByteArray().also { pending.reset() }
                }
                emulator.append(output, output.size)
                emulator.onScreenUpdate?.invoke()
            }
        }
    }

    fun appendLine(line: String) = append("$line\r\n".toByteArray())

    @Synchronized
    fun logText(): String = log.toString(Charsets.UTF_8.name())

    @Synchronized
    fun resize(columns: Int, rows: Int, cellWidth: Int, cellHeight: Int) {
        this.columns = columns
        this.rows = rows
        isReady = true
        emulator.resize(columns, rows, cellWidth, cellHeight)
        process?.resize(columns, rows)
        emulator.onScreenUpdate?.invoke()
    }

    /** Called on an IO thread. The child inherits a PTY, including stdin and stderr. */
    fun execute(command: String, globalMnt: Boolean = false): TerminalResult {
        var child: PtyProcess? = null
        return try {
            val argv = terminalShellCommand(command, globalMnt)
            child = synchronized(this) {
                check(!closed) { "Terminal session is closed" }
                PtyProcess(argv, columns, rows).also { process = it }
            }
            val buffer = ByteArray(8192)
            var drainedAfterExit = 0
            while (true) {
                val count = child.read(buffer)
                if (count < 0) break
                if (count > 0) append(buffer, count)
                if (child.pollExit() != null) {
                    // Background descendants may keep the slave open. Drain buffered output,
                    // then finish when the command itself exits, like Magisk's terminal.
                    drainedAfterExit += count
                    if (count == 0 || drainedAfterExit >= 1024 * 1024) break
                }
            }
            val code = child.waitFor()
            child = null // Already reaped; close the master in finally.
            TerminalResult(code)
        } catch (e: Exception) {
            Log.e("TerminalSession", "Command failed", e)
            appendLine("! ${e.message}")
            TerminalResult(-1, e.message.orEmpty())
        } finally {
            synchronized(this) {
                process?.close()
                process = null
            }
            // Reap even when reading failed or the screen was closed.
            child?.let { runCatching { it.waitFor() } }
        }
    }

    @Synchronized
    override fun close() {
        closed = true
        process?.close()
    }
}

data class TerminalResult(val code: Int, val err: String = "") {
    val isSuccess: Boolean get() = code == 0
}
