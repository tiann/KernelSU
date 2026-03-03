package me.weishu.kernelsu.ui.screen.module

import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.Dp
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.Navigator

@Composable
fun ModulePager(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ModulePagerMiuix(navigator, bottomInnerPadding)
        UiMode.Material -> ModulePagerMaterial(navigator, bottomInnerPadding)
    }
}
