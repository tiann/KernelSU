package me.weishu.kernelsu.ui.screen.executemoduleaction

import android.os.Environment
import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.repository.ModuleRepositoryImpl
import me.weishu.kernelsu.terminal.TerminalSession
import me.weishu.kernelsu.ui.terminal.TerminalViewModel
import me.weishu.kernelsu.ui.util.FlashResult
import me.weishu.kernelsu.ui.util.runModuleAction

@Composable
fun ExecuteModuleActionEffect(
    moduleId: String,
    viewModel: TerminalViewModel,
    onExit: () -> Unit
) {
    val context = LocalContext.current
    val noModule = stringResource(R.string.no_such_module)
    val moduleUnavailable = stringResource(R.string.module_unavailable)

    LaunchedEffect(viewModel.terminal.isReady) {
        if (!viewModel.terminal.isReady || viewModel.started) {
            return@LaunchedEffect
        }
        val repo = ModuleRepositoryImpl()
        val modules = repo.getModules().getOrDefault(emptyList())
        val moduleInfo = modules.find { info -> info.id == moduleId }
        if (moduleInfo == null) {
            Toast.makeText(context, noModule.format(moduleId), Toast.LENGTH_SHORT).show()
            onExit()
            return@LaunchedEffect
        }
        if (!moduleInfo.hasActionScript) {
            onExit()
            return@LaunchedEffect
        }
        if (!moduleInfo.enabled || moduleInfo.update || moduleInfo.remove) {
            Toast.makeText(context, moduleUnavailable.format(moduleInfo.name), Toast.LENGTH_SHORT).show()
            onExit()
            return@LaunchedEffect
        }
        viewModel.start { terminal ->
            FlashResult(runModuleAction(moduleId, terminal), false)
        }
    }
}

fun saveLog(
    terminal: TerminalSession,
    scope: CoroutineScope,
    showMessage: (String) -> Unit
): () -> Unit {
    return {
        scope.launch {
            val format = SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.getDefault())
            val date = format.format(Date())
            val file = File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                "KernelSU_module_action_log_${date}.log"
            )
            file.writeText(terminal.logText())
            showMessage("Log saved to ${file.absolutePath}")
        }
    }
}
