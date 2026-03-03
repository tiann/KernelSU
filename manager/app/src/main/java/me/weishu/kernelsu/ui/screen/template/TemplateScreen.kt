package me.weishu.kernelsu.ui.screen.template

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun AppProfileTemplateScreen() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppProfileTemplateScreenMiuix()
        UiMode.Material -> AppProfileTemplateScreenMaterial()
    }
}
