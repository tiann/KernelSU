package me.weishu.kernelsu.ui.screen.appprofile

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun AppProfileScreen(uid: Int, packageName: String) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppProfileScreenMiuix(uid, packageName)
        UiMode.Material -> AppProfileScreenMaterial(uid, packageName)
    }
}
