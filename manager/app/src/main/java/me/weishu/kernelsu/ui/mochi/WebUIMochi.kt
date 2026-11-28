package me.weishu.kernelsu.ui.mochi

import android.content.Intent
import androidx.activity.result.ActivityResultLauncher
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import kotlinx.collections.immutable.persistentListOf
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.webui.WebUIEvent
import me.weishu.kernelsu.ui.webui.WebUIState
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiTextField

/** JS alert / confirm / prompt and file chooser for module WebUI. */
@Composable
fun HandleWebUIEventMochi(webUIState: WebUIState, fileLauncher: ActivityResultLauncher<Intent>) {
    when (val event = webUIState.uiEvent) {
        is WebUIEvent.ShowAlert -> {
            var shown by remember(event) { mutableStateOf(true) }
            MochiDialog(
                visible = shown,
                onDismissRequest = {
                    if (shown) webUIState.onAlertResult()
                    shown = false
                },
                title = event.message,
                actions = persistentListOf(MochiDialogAction(stringResource(R.string.confirm), MochiDialogActionRole.Primary)),
            )
        }

        is WebUIEvent.ShowConfirm -> {
            var shown by remember(event) { mutableStateOf(true) }
            var answered by remember(event) { mutableStateOf(false) }
            MochiDialog(
                visible = shown,
                onDismissRequest = {
                    if (!answered) webUIState.onConfirmResult(false)
                    shown = false
                },
                title = event.message,
                actions = persistentListOf(
                    MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel, onClick = {
                        answered = true
                        webUIState.onConfirmResult(false)
                    }),
                    MochiDialogAction(stringResource(R.string.confirm), MochiDialogActionRole.Primary, onClick = {
                        answered = true
                        webUIState.onConfirmResult(true)
                    }),
                ),
            )
        }

        is WebUIEvent.ShowPrompt -> {
            var shown by remember(event) { mutableStateOf(true) }
            var answered by remember(event) { mutableStateOf(false) }
            var text by remember(event) { mutableStateOf(event.defaultValue) }
            MochiDialog(
                visible = shown,
                onDismissRequest = {
                    if (!answered) webUIState.onPromptResult(null)
                    shown = false
                },
                title = event.message,
                actions = persistentListOf(
                    MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel, onClick = {
                        answered = true
                        webUIState.onPromptResult(null)
                    }),
                    MochiDialogAction(stringResource(R.string.confirm), MochiDialogActionRole.Primary, onClick = {
                        answered = true
                        webUIState.onPromptResult(text)
                    }),
                ),
            ) {
                MochiTextField(value = text, onValueChange = { text = it }, modifier = Modifier.fillMaxWidth())
            }
        }

        is WebUIEvent.ShowFileChooser -> LaunchedEffect(event) {
            try {
                fileLauncher.launch(event.intent)
            } catch (_: Exception) {
                webUIState.onFileChooserResult(null)
            }
        }

        else -> Unit
    }
}
