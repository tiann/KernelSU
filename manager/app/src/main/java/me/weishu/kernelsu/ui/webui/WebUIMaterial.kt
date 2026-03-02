package me.weishu.kernelsu.ui.webui

import android.content.Intent
import androidx.activity.result.ActivityResultLauncher
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R

@Composable
fun HandleWebUIEventMaterial(
    webUIState: WebUIState,
    fileLauncher: ActivityResultLauncher<Intent>
) {
    when (val event = webUIState.uiEvent) {
        is WebUIEvent.ShowAlert -> {
            val showDialog = remember(event) { mutableStateOf(true) }
            if (showDialog.value) {
                AlertDialog(
                    onDismissRequest = {
                        webUIState.onAlertResult()
                        showDialog.value = false
                    },
                    confirmButton = {
                        TextButton(
                            onClick = {
                                webUIState.onAlertResult()
                                showDialog.value = false
                            },
                        ) {
                            Text(stringResource(R.string.confirm))
                        }
                    },
                    text = {
                        Text(event.message)
                    }
                )
            }
        }

        is WebUIEvent.ShowConfirm -> {
            val showDialog = remember(event) { mutableStateOf(true) }
            if (showDialog.value) {
                AlertDialog(
                    onDismissRequest = {
                        webUIState.onConfirmResult(false)
                        showDialog.value = false
                    },
                    confirmButton = {
                        TextButton(
                            onClick = {
                                webUIState.onConfirmResult(true)
                                showDialog.value = false
                            },
                        ) {
                            Text(stringResource(R.string.confirm))
                        }
                    },
                    dismissButton = {
                        TextButton(
                            onClick = {
                                webUIState.onConfirmResult(false)
                                showDialog.value = false
                            },
                        ) {
                            Text(stringResource(android.R.string.cancel))
                        }
                    },
                    text = {
                        Text(event.message)
                    }
                )
            }
        }

        is WebUIEvent.ShowPrompt -> {
            val showDialog = remember(event) { mutableStateOf(true) }
            val state = remember(event) { mutableStateOf(event.defaultValue) }
            if (showDialog.value) {
                AlertDialog(
                    onDismissRequest = {
                        webUIState.onPromptResult(null)
                        showDialog.value = false
                    },
                    confirmButton = {
                        TextButton(
                            onClick = {
                                webUIState.onPromptResult(state.value)
                                showDialog.value = false
                            },
                        ) {
                            Text(stringResource(R.string.confirm))
                        }
                    },
                    dismissButton = {
                        TextButton(
                            onClick = {
                                webUIState.onPromptResult(null)
                                showDialog.value = false
                            },
                        ) {
                            Text(stringResource(android.R.string.cancel))
                        }
                    },
                    text = {
                        Column {
                            OutlinedTextField(
                                label = { Text(event.message) },
                                value = state.value,
                                onValueChange = { state.value = it },
                                modifier = Modifier.fillMaxWidth()
                            )
                        }
                    }
                )
            }
        }

        is WebUIEvent.ShowFileChooser -> {
            LaunchedEffect(event) {
                try {
                    fileLauncher.launch(event.intent)
                } catch (_: Exception) {
                    webUIState.onFileChooserResult(null)
                }
            }
        }

        else -> {}
    }
}
