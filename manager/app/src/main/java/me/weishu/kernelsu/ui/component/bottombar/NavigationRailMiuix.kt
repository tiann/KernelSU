package me.weishu.kernelsu.ui.component.bottombar

import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.rootAvailable
import top.yukonga.miuix.kmp.basic.NavigationRail
import top.yukonga.miuix.kmp.basic.NavigationRailItem
import top.yukonga.miuix.kmp.blur.LayerBackdrop
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun NavigationRailMiuix(
    blurBackdrop: LayerBackdrop?,
    modifier: Modifier = Modifier,
) {
    val isManager = Natives.isManager
    val fullFeatured = isManager && !Natives.requireNewKernel() && rootAvailable()
    if (!fullFeatured) return

    val mainState = LocalMainPagerState.current

    val items = BottomBarDestination.entries.map { destination ->
        Pair(stringResource(destination.label), destination.icon)
    }

    BlurredBar(blurBackdrop) {
        NavigationRail(
            modifier = modifier
                .fillMaxHeight(),
            color = if (blurBackdrop != null) Color.Transparent else MiuixTheme.colorScheme.surface,
        ) {
            Spacer(modifier = Modifier.weight(1f))
            items.forEachIndexed { index, (label, icon) ->
                NavigationRailItem(
                    icon = icon,
                    label = label,
                    selected = mainState.selectedPage == index,
                    onClick = {
                        mainState.animateToPage(index)
                    },
                    modifier = Modifier.padding(vertical = 4.dp)
                )
            }
            Spacer(modifier = Modifier.weight(1f))
        }
    }
}
