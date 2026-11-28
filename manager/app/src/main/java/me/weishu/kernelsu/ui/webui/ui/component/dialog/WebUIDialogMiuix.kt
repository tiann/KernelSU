package me.weishu.kernelsu.ui.webui.ui.component.dialog

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.input.TextFieldState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.webui.model.WebUIDialog
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.window.WindowDialog

@Composable
fun WebUIDialogMiuix(
    dialog: WebUIDialog,
) {
    when (dialog) {
        is WebUIDialog.Alert -> {
            WindowDialog(
                show = true,
                onDismissRequest = dialog.onDismiss,
                content = {
                    Column {
                        Text(dialog.message)
                        Spacer(Modifier.height(12.dp))
                        TextButton(
                            modifier = Modifier.fillMaxWidth(),
                            onClick = dialog.onConfirm,
                            text = stringResource(R.string.confirm),
                            colors = ButtonDefaults.textButtonColorsPrimary()
                        )
                    }
                }
            )
        }

        is WebUIDialog.Confirm -> {
            WindowDialog(
                show = true,
                onDismissRequest = dialog.onDismiss,
                content = {
                    Column {
                        Text(dialog.message)
                        Spacer(Modifier.height(12.dp))
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            TextButton(
                                onClick = dialog.onDismiss,
                                text = stringResource(android.R.string.cancel),
                                modifier = Modifier.weight(1f),
                            )
                            Spacer(modifier = Modifier.width(20.dp))
                            TextButton(
                                onClick = dialog.onConfirm,
                                text = stringResource(R.string.confirm),
                                modifier = Modifier.weight(1f),
                                colors = ButtonDefaults.textButtonColorsPrimary()
                            )
                        }
                    }
                }
            )
        }

        is WebUIDialog.Prompt -> {
            val inputState = remember(dialog) { TextFieldState(dialog.defaultValue) }
            WindowDialog(
                show = true,
                onDismissRequest = dialog.onDismiss,
                content = {
                    Column {
                        Text(dialog.message)
                        Spacer(Modifier.height(12.dp))
                        TextField(
                            modifier = Modifier.padding(bottom = 16.dp),
                            state = inputState
                        )
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            TextButton(
                                onClick = dialog.onDismiss,
                                text = stringResource(android.R.string.cancel),
                                modifier = Modifier.weight(1f),
                            )
                            Spacer(modifier = Modifier.width(20.dp))
                            TextButton(
                                onClick = { dialog.onConfirm(inputState.text.toString()) },
                                text = stringResource(R.string.confirm),
                                modifier = Modifier.weight(1f),
                                colors = ButtonDefaults.textButtonColorsPrimary()
                            )
                        }
                    }
                }
            )
        }
    }
}
