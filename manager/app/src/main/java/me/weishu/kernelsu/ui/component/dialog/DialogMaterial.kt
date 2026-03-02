package me.weishu.kernelsu.ui.component.dialog

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.LoadingIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.ui.component.GithubMarkdown
import me.weishu.kernelsu.ui.component.Markdown

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun LoadingDialogMaterial(showDialog: MutableState<Boolean>) {
    if (showDialog.value) {
        AlertDialog(
            onDismissRequest = { },
            confirmButton = { },
            text = {
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    LoadingIndicator()
                }
            }
        )
    }
}

@Composable
fun ConfirmDialogMaterial(
    visuals: ConfirmDialogVisuals,
    confirm: () -> Unit,
    dismiss: () -> Unit,
    showDialog: MutableState<Boolean>
) {
    if (showDialog.value) {
        AlertDialog(
            onDismissRequest = {
                dismiss()
                showDialog.value = false
            },
            title = { Text(visuals.title) },
            text = {
                visuals.content?.let { content ->
                    when {
                        visuals.isMarkdown -> Markdown(content = content)
                        visuals.isHtml -> GithubMarkdown(content = content, containerColor = MaterialTheme.colorScheme.surfaceContainerHigh)
                        else -> Text(text = content)
                    }
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        confirm()
                        showDialog.value = false
                    }
                ) {
                    Text(visuals.confirm ?: stringResource(id = android.R.string.ok))
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        dismiss()
                        showDialog.value = false
                    }
                ) {
                    Text(visuals.dismiss ?: stringResource(id = android.R.string.cancel))
                }
            }
        )
    }
}
