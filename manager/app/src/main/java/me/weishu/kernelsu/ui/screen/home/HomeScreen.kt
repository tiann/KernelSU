package me.weishu.kernelsu.ui.screen.home

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.unit.Dp
import androidx.lifecycle.viewmodel.compose.viewModel
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.viewmodel.HomeViewModel

@Composable
fun HomePager(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    val viewModel = viewModel<HomeViewModel>()
    val uiState by viewModel.uiState.collectAsState()
    val mainState = LocalMainPagerState.current
    val uriHandler = LocalUriHandler.current
    val systemInfo = getSystemInfo()

    LaunchedEffect(Unit) {
        viewModel.refresh()
        viewModel.updateSystemInfo(systemInfo)
    }

    val actions = HomeActions(
        onInstallClick = { navigator.push(Route.Install) },
        onSuperuserClick = { mainState.animateToPage(1) },
        onModuleClick = { mainState.animateToPage(2) },
        onOpenUrl = uriHandler::openUri,
    )

    when (LocalUiMode.current) {
        UiMode.Miuix -> HomePagerMiuix(
            state = uiState,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )

        UiMode.Material -> HomePagerMaterial(
            state = uiState,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )
    }
}
