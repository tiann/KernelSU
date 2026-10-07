package me.weishu.kernelsu.ui.terminal

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.terminal.TerminalSession
import me.weishu.kernelsu.ui.util.FlashResult

class TerminalViewModel(private val savedState: SavedStateHandle) : ViewModel() {
    val terminal = TerminalSession()
    var started by mutableStateOf(savedState["started"] ?: false)
        private set
    var result by mutableStateOf(
        if (started) FlashResult(
            savedState["code"] ?: -1,
            savedState["error"] ?: "Terminal process was interrupted",
            savedState["reboot"] ?: false,
        ) else null
    )
        private set

    init {
        // Never repeat a flash/action after Android restores a killed app process.
        if (started) {
            savedState.get<String>("log")?.let { terminal.append(it.toByteArray()) }
            if (!savedState.contains("code")) terminal.appendLine("! Terminal process was interrupted")
        }
    }

    fun start(action: (TerminalSession) -> FlashResult) {
        if (started) return
        started = true
        savedState["started"] = true
        viewModelScope.launch {
            result = withContext(Dispatchers.IO) {
                try {
                    action(terminal)
                } catch (e: Exception) {
                    FlashResult(-1, e.message.orEmpty(), false)
                }.also {
                    if (it.code != 0) {
                        terminal.appendLine("Error code: ${it.code}. ${it.err} Please save and check the log.")
                    }
                }
            }
            result?.let {
                savedState["code"] = it.code
                savedState["error"] = it.err
                savedState["reboot"] = it.showReboot
                // Keep saved instance state bounded; the live session retains the full log.
                savedState["log"] = terminal.logText().takeLast(16 * 1024)
            }
        }
    }

    override fun onCleared() {
        terminal.close()
    }
}
