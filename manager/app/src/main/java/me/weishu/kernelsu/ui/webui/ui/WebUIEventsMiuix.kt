package me.weishu.kernelsu.ui.webui.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.input.rememberTextFieldState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIOverlay
import me.weishu.kernelsu.ui.webui.model.WebUIState
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.window.WindowDialog

@Composable
fun HandleWebUIEventMiuix(
    state: WebUIState,
    dispatch: (WebUIIntent) -> Unit,
) {
    when (val overlay = state.overlay) {
        is WebUIOverlay.Alert -> {
            WindowDialog(
                show = true,
                content = {
                    Column {
                        Text(overlay.message)
                        Spacer(Modifier.height(12.dp))
                        TextButton(
                            modifier = Modifier.fillMaxWidth(),
                            onClick = { dispatch(WebUIIntent.AlertConfirmed) },
                            text = stringResource(R.string.confirm),
                            colors = ButtonDefaults.textButtonColorsPrimary()
                        )
                    }
                }
            )
        }

        is WebUIOverlay.Confirm -> {
            WindowDialog(
                show = true,
                onDismissRequest = { dispatch(WebUIIntent.ConfirmAnswered(false)) },
                content = {
                    Column {
                        Text(overlay.message)
                        Spacer(Modifier.height(12.dp))
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            TextButton(
                                onClick = { dispatch(WebUIIntent.ConfirmAnswered(false)) },
                                text = stringResource(android.R.string.cancel),
                                modifier = Modifier.weight(1f),
                            )
                            Spacer(modifier = Modifier.width(20.dp))
                            TextButton(
                                onClick = { dispatch(WebUIIntent.ConfirmAnswered(true)) },
                                text = stringResource(R.string.confirm),
                                modifier = Modifier.weight(1f),
                                colors = ButtonDefaults.textButtonColorsPrimary()
                            )
                        }
                    }
                }
            )
        }

        is WebUIOverlay.Prompt -> {
            val inputState = rememberTextFieldState(overlay.defaultValue)
            WindowDialog(
                show = true,
                onDismissRequest = { dispatch(WebUIIntent.PromptAnswered(null)) },
                content = {
                    Column {
                        Text(overlay.message)
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
                                onClick = { dispatch(WebUIIntent.PromptAnswered(null)) },
                                text = stringResource(android.R.string.cancel),
                                modifier = Modifier.weight(1f),
                            )
                            Spacer(modifier = Modifier.width(20.dp))
                            TextButton(
                                onClick = { dispatch(WebUIIntent.PromptAnswered(inputState.text.toString())) },
                                text = stringResource(R.string.confirm),
                                modifier = Modifier.weight(1f),
                                colors = ButtonDefaults.textButtonColorsPrimary()
                            )
                        }
                    }
                }
            )
        }

        null -> Unit
    }
}
