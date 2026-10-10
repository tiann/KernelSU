package me.weishu.kernelsu.ui.webui.ui.component.dialog

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
import me.weishu.kernelsu.ui.webui.model.WebUIDialog

@Composable
fun WebUIDialogMaterial(
    dialog: WebUIDialog,
) {
    when (dialog) {
        is WebUIDialog.Alert -> {
            AlertDialog(
                onDismissRequest = dialog.onDismiss,
                confirmButton = {
                    TextButton(onClick = dialog.onConfirm) {
                        Text(stringResource(R.string.confirm))
                    }
                },
                text = { Text(dialog.message) }
            )
        }

        is WebUIDialog.Confirm -> {
            AlertDialog(
                onDismissRequest = dialog.onDismiss,
                confirmButton = {
                    TextButton(onClick = dialog.onConfirm) {
                        Text(stringResource(R.string.confirm))
                    }
                },
                dismissButton = {
                    TextButton(onClick = dialog.onDismiss) {
                        Text(stringResource(android.R.string.cancel))
                    }
                },
                text = { Text(dialog.message) }
            )
        }

        is WebUIDialog.Prompt -> {
            val inputState = remember(dialog) { mutableStateOf(dialog.defaultValue) }
            AlertDialog(
                onDismissRequest = dialog.onDismiss,
                confirmButton = {
                    TextButton(onClick = { dialog.onConfirm(inputState.value) }) {
                        Text(stringResource(R.string.confirm))
                    }
                },
                dismissButton = {
                    TextButton(onClick = dialog.onDismiss) {
                        Text(stringResource(android.R.string.cancel))
                    }
                },
                text = {
                    Column {
                        OutlinedTextField(
                            label = { Text(dialog.message) },
                            value = inputState.value,
                            onValueChange = { inputState.value = it },
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                }
            )
        }
    }
}
