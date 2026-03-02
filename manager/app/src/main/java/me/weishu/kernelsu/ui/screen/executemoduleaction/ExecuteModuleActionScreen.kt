package me.weishu.kernelsu.ui.screen.executemoduleaction

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun ExecuteModuleActionScreen(moduleId: String) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ExecuteModuleActionScreenMiuix(moduleId)
        UiMode.Material -> ExecuteModuleActionScreenMaterial(moduleId)
    }
}
