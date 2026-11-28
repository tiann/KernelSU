package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigationevent.NavigationEventInfo
import androidx.navigationevent.compose.NavigationBackHandler
import androidx.navigationevent.compose.rememberNavigationEventState
import kotlinx.collections.immutable.toImmutableList
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.component.bottombar.rememberMainPagerState
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.home.HomePager
import me.weishu.kernelsu.ui.screen.module.ModulePager
import me.weishu.kernelsu.ui.screen.settings.SettingPager
import me.weishu.kernelsu.ui.screen.superuser.SuperUserPager
import me.weishu.kernelsu.ui.theme.LocalEnableNavigationBadge
import me.weishu.kernelsu.ui.util.getSuperuserCount
import me.weishu.kernelsu.ui.viewmodel.MainPagerConfig
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import ui.feice.mochi.basic.MochiTabBarDefaults
import ui.feice.mochi.basic.MochiTabItem
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.navigation.MochiTabScaffold
import ui.feice.mochi.navigation.rememberMochiTabScaffoldState

private val TabIcons = listOf("home", "shield-checkmark", "extension-puzzle", "settings")
private val TabLabels = listOf(R.string.home, R.string.superuser, R.string.module, R.string.settings)

/**
 * Root screen of the Mochi UI with a tab bar. Each tab keeps its scroll position, tapping the
 * current tab scrolls to top, and the bar hides when a page is pushed.
 * [LocalMainPagerState] is kept in sync with the tab bar in both directions.
 */
@Composable
fun MochiMainScreen(initialPage: Int, onPageChanged: (Int) -> Unit) {
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val pagerState = rememberPagerState(initialPage = initialPage, pageCount = { MainPagerConfig.PAGE_COUNT })
    val mainPagerState = rememberMainPagerState(pagerState = pagerState, animatePageChanges = false)
    val tabState = rememberMochiTabScaffoldState(initialPage)

    LaunchedEffect(mainPagerState) {
        snapshotFlow { mainPagerState.selectedPage }.collect { page ->
            if (tabState.selectedIndex != page) tabState.select(page)
        }
    }
    LaunchedEffect(initialPage) {
        if (tabState.selectedIndex != initialPage) tabState.select(initialPage)
    }
    LaunchedEffect(tabState) {
        snapshotFlow { tabState.selectedIndex }.collect { page ->
            onPageChanged(page)
            mainPagerState.animateToPage(page)
        }
    }

    // Back goes to the home tab first
    val backToHome by remember {
        derivedStateOf { navigator.current() is Route.Main && navigator.backStackSize() == 1 && tabState.selectedIndex != 0 }
    }
    NavigationBackHandler(
        state = rememberNavigationEventState(NavigationEventInfo.None),
        isBackEnabled = backToHome,
        onBackCompleted = { tabState.select(0) },
    )

    // Preload modules and superuser list for the badges
    val isFullFeatured = Natives.isFullFeatured()
    val badgeEnabled = LocalEnableNavigationBadge.current && isFullFeatured
    val moduleViewModel = viewModel<ModuleViewModel>()
    val moduleUiState by moduleViewModel.uiState.collectAsStateWithLifecycle()
    val superUserViewModel = viewModel<SuperUserViewModel>()
    val grantedUidCount by remember(superUserViewModel) {
        superUserViewModel.uiState.map { state -> state.groupedApps.count { it.anyAllowSu } }.distinctUntilChanged()
    }.collectAsStateWithLifecycle(0)
    var preloaded by rememberSaveable { mutableStateOf(false) }
    LaunchedEffect(isFullFeatured) {
        if (!isFullFeatured || preloaded) return@LaunchedEffect
        moduleViewModel.initializePreferences()
        val moduleState = moduleViewModel.uiState.value
        if (!moduleState.hasLoaded) {
            if (!moduleState.isRefreshing) moduleViewModel.fetchModuleList()
            moduleViewModel.uiState.first { it.hasLoaded }
        }
        moduleViewModel.syncModuleUpdateInfo(moduleViewModel.uiState.value.modules)
        val superUserState = superUserViewModel.uiState.value
        if (!superUserState.hasLoaded) {
            superUserViewModel.initializePreferences()
            if (superUserState.isRefreshing) {
                superUserViewModel.uiState.first { it.hasLoaded }
            } else {
                superUserViewModel.loadAppList().join()
            }
        }
        preloaded = true
    }
    var superuserCount by remember { mutableIntStateOf(0) }
    LaunchedEffect(badgeEnabled, grantedUidCount) {
        superuserCount = if (badgeEnabled) withContext(Dispatchers.IO) { getSuperuserCount() } else 0
    }
    val moduleUpdates = moduleUiState.updateInfo.count { it.value.downloadUrl.isNotBlank() }
    val newUi = MochiNews.unseen(context)

    val labels = TabLabels.map { stringResource(it) }
    val items = labels.mapIndexed { index, label ->
        val badge = when (index) {
            1 -> superuserCount.takeIf { badgeEnabled && it > 0 }?.toString()
            2 -> moduleUpdates.takeIf { badgeEnabled && it > 0 }?.toString()
            3 -> if (newUi) "" else null
            else -> null
        }
        MochiTabItem(label = label, badge = badge) { tint -> MochiIonicon(TabIcons[index], tint = tint, size = 22.dp) }
    }.toImmutableList()

    CompositionLocalProvider(LocalMainPagerState provides mainPagerState) {
        MochiTabScaffold(
            items = items,
            state = tabState,
            collapseOnScroll = false,
            behavior = MochiTabBarDefaults.iconOnlyBehavior(selectedLabelOnly = true),
        ) { index ->
            val isCurrent = tabState.selectedIndex == index
            when (index) {
                0 -> HomePager(navigator, 0.dp, isCurrent)
                1 -> SuperUserPager(navigator, 0.dp, isCurrent)
                2 -> ModulePager(0.dp, isCurrent)
                else -> SettingPager(navigator, 0.dp, isCurrent)
            }
        }
    }
}
