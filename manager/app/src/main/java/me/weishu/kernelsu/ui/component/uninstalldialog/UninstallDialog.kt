package me.weishu.kernelsu.ui.component.uninstalldialog

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun UninstallDialog(showDialog: MutableState<Boolean>) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> UninstallDialogMiuix(showDialog)
        UiMode.Material -> UninstallDialogMaterial(showDialog)
    }
}
