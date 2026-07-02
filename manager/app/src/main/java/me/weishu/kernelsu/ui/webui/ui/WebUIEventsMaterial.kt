package me.weishu.kernelsu.ui.webui.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIOverlay
import me.weishu.kernelsu.ui.webui.model.WebUIState

@Composable
fun HandleWebUIEventMaterial(
    state: WebUIState,
    dispatch: (WebUIIntent) -> Unit,
) {
    when (val overlay = state.overlay) {
        is WebUIOverlay.Alert -> {
            AlertDialog(
                onDismissRequest = { dispatch(WebUIIntent.AlertConfirmed) },
                confirmButton = {
                    TextButton(onClick = { dispatch(WebUIIntent.AlertConfirmed) }) {
                        Text(stringResource(R.string.confirm))
                    }
                },
                text = { Text(overlay.message) }
            )
        }

        is WebUIOverlay.Confirm -> {
            AlertDialog(
                onDismissRequest = { dispatch(WebUIIntent.ConfirmAnswered(false)) },
                confirmButton = {
                    TextButton(onClick = { dispatch(WebUIIntent.ConfirmAnswered(true)) }) {
                        Text(stringResource(R.string.confirm))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { dispatch(WebUIIntent.ConfirmAnswered(false)) }) {
                        Text(stringResource(android.R.string.cancel))
                    }
                },
                text = { Text(overlay.message) }
            )
        }

        is WebUIOverlay.Prompt -> {
            val inputState = remember(overlay) { mutableStateOf(overlay.defaultValue) }
            AlertDialog(
                onDismissRequest = { dispatch(WebUIIntent.PromptAnswered(null)) },
                confirmButton = {
                    TextButton(onClick = { dispatch(WebUIIntent.PromptAnswered(inputState.value)) }) {
                        Text(stringResource(R.string.confirm))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { dispatch(WebUIIntent.PromptAnswered(null)) }) {
                        Text(stringResource(android.R.string.cancel))
                    }
                },
                text = {
                    Column {
                        OutlinedTextField(
                            label = { Text(overlay.message) },
                            value = inputState.value,
                            onValueChange = { inputState.value = it },
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                }
            )
        }

        null -> Unit
    }
}
