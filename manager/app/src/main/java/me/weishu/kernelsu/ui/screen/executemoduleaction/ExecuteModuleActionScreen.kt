package me.weishu.kernelsu.ui.screen.executemoduleaction

import androidx.activity.compose.LocalActivity
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.dropUnlessResumed
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator

@Composable
fun ExecuteModuleActionScreen(moduleId: String, fromShortcut: Boolean = false) {
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val activity = LocalActivity.current
    val scope = rememberCoroutineScope()
    var text by rememberSaveable { mutableStateOf("") }
    val logContent = rememberSaveable { StringBuilder() }
    var isComplete by rememberSaveable { mutableStateOf(false) }

    val exitExecute = {
        if (fromShortcut && activity != null) {
            activity.finishAndRemoveTask()
        } else {
            navigator.pop()
        }
    }

    ExecuteModuleActionEffect(
        moduleId = moduleId,
        text = text,
        logContent = logContent,
        fromShortcut = fromShortcut,
        onTextUpdate = { text = it },
        onComplete = { isComplete = true },
        onExit = exitExecute
    )

    val state = ExecuteModuleActionUiState(
        text = text,
        isComplete = isComplete,
    )
    val actions = ExecuteModuleActionScreenActions(
        onBack = dropUnlessResumed { navigator.pop() },
        onSaveLog = saveLog(logContent, context, scope),
        onClose = exitExecute,
    )

    when (LocalUiMode.current) {
        UiMode.Miuix -> ExecuteModuleActionScreenMiuix(state, actions)
        UiMode.Material -> ExecuteModuleActionScreenMaterial(state, actions)
    }
}
