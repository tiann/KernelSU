package me.weishu.kernelsu.ui.component.uninstalldialog

import androidx.compose.runtime.Composable

@Composable
fun UninstallDialog(
    show: Boolean,
    onDismissRequest: () -> Unit
) {
    UninstallDialogMaterial(show, onDismissRequest)
}
