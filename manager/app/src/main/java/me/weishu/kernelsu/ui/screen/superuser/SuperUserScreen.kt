package me.weishu.kernelsu.ui.screen.superuser

import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.Dp
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.Navigator

@Composable
fun SuperUserPager(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SuperUserPagerMiuix(navigator, bottomInnerPadding)
        UiMode.Material -> SuperUserPagerMaterial(navigator, bottomInnerPadding)
    }
}
