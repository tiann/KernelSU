package me.weishu.kernelsu.ui.component.bottombar

import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.util.defaultHazeEffect
import me.weishu.kernelsu.ui.util.rootAvailable
import top.yukonga.miuix.kmp.basic.NavigationRail
import top.yukonga.miuix.kmp.basic.NavigationRailItem
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun NavigationRailMiuix(
    hazeState: HazeState,
    hazeStyle: HazeStyle,
    modifier: Modifier = Modifier,
) {
    val isManager = Natives.isManager
    val fullFeatured = isManager && !Natives.requireNewKernel() && rootAvailable()
    if (!fullFeatured) return

    val mainState = LocalMainPagerState.current
    val enableBlur = LocalEnableBlur.current

    val items = BottomBarDestination.entries.map { destination ->
        Pair(stringResource(destination.label), destination.icon)
    }

    NavigationRail(
        modifier = modifier
            .fillMaxHeight()
            .then(
                if (enableBlur) {
                    Modifier.defaultHazeEffect(hazeState, hazeStyle)
                } else Modifier
            ),
        color = if (enableBlur) Color.Transparent else MiuixTheme.colorScheme.surface,
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
