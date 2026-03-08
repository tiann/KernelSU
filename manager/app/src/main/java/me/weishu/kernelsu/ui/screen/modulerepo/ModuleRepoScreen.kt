package me.weishu.kernelsu.ui.screen.modulerepo

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun ModuleRepoScreen() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ModuleRepoScreenMiuix()
        UiMode.Material -> ModuleRepoScreenMaterial()
    }
}

@Composable
fun ModuleRepoDetailScreen(module: RepoModuleArg) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ModuleRepoDetailScreenMiuix(module)
        UiMode.Material -> ModuleRepoDetailScreenMaterial(module)
    }
}
