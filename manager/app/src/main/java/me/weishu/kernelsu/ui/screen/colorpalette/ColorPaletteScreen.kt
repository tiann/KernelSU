package me.weishu.kernelsu.ui.screen.colorpalette

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun ColorPaletteScreen() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ColorPaletteScreenMiuix()
        UiMode.Material -> ColorPaletteScreenMaterial()
    }
}
