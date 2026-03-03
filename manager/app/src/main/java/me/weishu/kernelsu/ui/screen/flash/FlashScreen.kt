package me.weishu.kernelsu.ui.screen.flash

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun FlashScreen(flashIt: FlashIt) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> FlashScreenMiuix(flashIt)
        UiMode.Material -> FlashScreenMaterial(flashIt)
    }
}
