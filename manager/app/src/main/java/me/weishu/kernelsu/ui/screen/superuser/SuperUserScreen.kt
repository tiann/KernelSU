package me.weishu.kernelsu.ui.screen.superuser

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.Dp
import androidx.core.content.edit
import androidx.lifecycle.viewmodel.compose.viewModel
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel

@Composable
fun SuperUserPager(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    val viewModel = viewModel<SuperUserViewModel>()
    val uiState by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)

    LaunchedEffect(Unit) {
        if (uiState.groupedApps.isEmpty()) {
            viewModel.setShowSystemApps(prefs.getBoolean("show_system_apps", false)).join()
            viewModel.setShowOnlyPrimaryUserApps(prefs.getBoolean("show_only_primary_user_apps", false)).join()
            viewModel.loadAppList().join()
        } else if (viewModel.isNeedRefresh) {
            viewModel.loadAppList(resort = false).join()
        }
    }

    val onSearchTextChange: (String) -> Unit = viewModel::updateSearchText
    val onToggleShowSystemApps: () -> Unit = {
        val newValue = !uiState.showSystemApps
        viewModel.setShowSystemApps(newValue)
        prefs.edit { putBoolean("show_system_apps", newValue) }
    }
    val onToggleShowOnlyPrimaryUserApps: () -> Unit = {
        val newValue = !uiState.showOnlyPrimaryUserApps
        viewModel.setShowOnlyPrimaryUserApps(newValue)
        prefs.edit { putBoolean("show_only_primary_user_apps", newValue) }
    }
    val onOpenProfile: (GroupedApps) -> Unit = { group ->
        navigator.push(Route.AppProfile(group.uid))
        viewModel.markNeedRefresh()
    }
    val actions = SuperUserActions(
        onRefresh = { viewModel.loadAppList(force = true) },
        onSearchTextChange = onSearchTextChange,
        onSearchStatusChange = viewModel::updateSearchStatus,
        onClearSearch = { onSearchTextChange("") },
        onToggleShowSystemApps = onToggleShowSystemApps,
        onToggleShowOnlyPrimaryUserApps = onToggleShowOnlyPrimaryUserApps,
        onOpenProfile = onOpenProfile,
    )

    when (LocalUiMode.current) {
        UiMode.Miuix -> SuperUserPagerMiuix(
            uiState = uiState,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )

        UiMode.Material -> SuperUserPagerMaterial(
            uiState = uiState,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )
    }
}
