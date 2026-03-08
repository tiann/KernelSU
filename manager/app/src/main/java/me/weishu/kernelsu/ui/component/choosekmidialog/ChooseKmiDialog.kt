package me.weishu.kernelsu.ui.component.choosekmidialog

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun ChooseKmiDialog(
    showDialog: MutableState<Boolean>,
    onSelected: (String?) -> Unit
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ChooseKmiDialogMiuix(showDialog, onSelected)
        UiMode.Material -> ChooseKmiDialogMaterial(showDialog, onSelected)
    }
}
