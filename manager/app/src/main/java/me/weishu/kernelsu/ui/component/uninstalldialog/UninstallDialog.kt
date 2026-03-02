package me.weishu.kernelsu.ui.component.uninstalldialog

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.Navigator

@Composable
fun UninstallDialog(
    showDialog: MutableState<Boolean>,
    navigator: Navigator
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> UninstallDialogMiuix(showDialog, navigator)
        UiMode.Material -> UninstallDialogMaterial(showDialog, navigator)
    }
}
