package me.weishu.kernelsu.ui.screen.install

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun InstallScreen() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> InstallScreenMiuix()
        UiMode.Material -> InstallScreenMaterial()
    }
}
