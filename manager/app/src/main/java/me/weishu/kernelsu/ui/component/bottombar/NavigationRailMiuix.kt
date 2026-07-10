package me.weishu.kernelsu.ui.component.bottombar

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.util.rootAvailable
import top.yukonga.miuix.kmp.basic.NavigationRail
import top.yukonga.miuix.kmp.basic.NavigationRailItem
import top.yukonga.miuix.kmp.basic.rememberNavigationRailState
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun NavigationRailMiuix(
    moduleBadge: ModuleBadgeState,
    modifier: Modifier = Modifier,
) {
    val isManager = Natives.isManager
    val fullFeatured = isManager && !Natives.requireNewKernel() && rootAvailable()
    if (!fullFeatured) return

    val mainState = LocalMainPagerState.current

    val items = BottomBarDestination.entries.map { destination ->
        Pair(stringResource(destination.label), destination.icon)
    }

    NavigationRail(
        modifier = modifier,
        state = rememberNavigationRailState(),
        color = MiuixTheme.colorScheme.surface,
        expandContentDescription = stringResource(R.string.nav_rail_expand),
        collapseContentDescription = stringResource(R.string.nav_rail_collapse),
    ) {
        items.forEachIndexed { index, (label, icon) ->
            NavigationRailItem(
                selected = mainState.selectedPage == index,
                onClick = {
                    mainState.animateToPage(index)
                },
                icon = icon,
                label = label,
                badge = moduleBadgeFor(index, moduleBadge),
            )
        }
    }
}
