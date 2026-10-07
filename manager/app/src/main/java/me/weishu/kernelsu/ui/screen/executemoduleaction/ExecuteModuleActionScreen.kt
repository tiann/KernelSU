package me.weishu.kernelsu.ui.screen.executemoduleaction

import android.widget.Toast
import androidx.activity.compose.LocalActivity
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.terminal.TerminalViewModel

@Composable
fun ExecuteModuleActionScreen(moduleId: String, fromShortcut: Boolean = false) {
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val activity = LocalActivity.current
    val scope = rememberCoroutineScope()
    val terminalViewModel: TerminalViewModel = viewModel()
    val terminal = terminalViewModel.terminal
    val isComplete = terminalViewModel.result != null
    val uiMode = LocalUiMode.current
    val snackbarHost = remember { SnackbarHostState() }
    val exitExecute = {
        if (fromShortcut && activity != null) {
            activity.finishAndRemoveTask()
        } else {
            navigator.pop()
        }
    }

    fun showMessage(message: String) {
        scope.launch {
            if (uiMode == UiMode.Material) {
                snackbarHost.showSnackbar(message)
            } else {
                Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
            }
        }
    }

    // Always auto disable from shortcuts
    LaunchedEffect(isComplete) {
        if (isComplete) {
            if (fromShortcut) {
                if (terminalViewModel.result?.code == 0) {
                    Toast.makeText(context, R.string.module_action_success, Toast.LENGTH_SHORT).show()
                }
                exitExecute()
            }
        }
    }

    ExecuteModuleActionEffect(
        moduleId = moduleId,
        viewModel = terminalViewModel,
        onExit = exitExecute
    )

    val state = ExecuteModuleActionUiState(
        isComplete = isComplete,
    )
    val actions = ExecuteModuleActionScreenActions(
        onBack = dropUnlessResumed { navigator.pop() },
        onSaveLog = saveLog(terminal, scope) { showMessage(it) },
        onClose = exitExecute,
    )

    when (uiMode) {
        UiMode.Miuix -> ExecuteModuleActionScreenMiuix(state, actions, terminal)
        UiMode.Material -> ExecuteModuleActionScreenMaterial(state, actions, snackbarHost, terminal)
    }
}
