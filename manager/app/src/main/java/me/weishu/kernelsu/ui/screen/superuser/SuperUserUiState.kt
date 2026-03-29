package me.weishu.kernelsu.ui.screen.superuser

import androidx.compose.runtime.Immutable
import androidx.compose.runtime.Stable
import androidx.compose.ui.graphics.Color
import me.weishu.kernelsu.data.model.AppInfo
import me.weishu.kernelsu.ui.component.SearchStatus

@Immutable
data class GroupedApps(
    val uid: Int,
    val apps: List<AppInfo>,
    val primary: AppInfo,
    val anyAllowSu: Boolean,
    val anyCustom: Boolean,
    val shouldUmount: Boolean,
    val ownerName: String? = null,
    val matchedPackageNames: Set<String> = emptySet(),
)

@Stable
data class SuperUserUiState(
    val isRefreshing: Boolean = false,
    val hasLoaded: Boolean = false,
    val groupedApps: List<GroupedApps> = emptyList(),
    val userIds: List<Int> = emptyList(),
    val searchStatus: SearchStatus = SearchStatus(""),
    val searchResults: List<GroupedApps> = emptyList(),
    val showSystemApps: Boolean = false,
    val showOnlyPrimaryUserApps: Boolean = false,
    val error: Throwable? = null
)

@Immutable
data class SuperUserActions(
    val onRefresh: () -> Unit,
    val onOpenSulog: () -> Unit,
    val onSearchTextChange: (String) -> Unit,
    val onSearchStatusChange: (SearchStatus) -> Unit,
    val onClearSearch: () -> Unit,
    val onToggleShowSystemApps: () -> Unit,
    val onToggleShowOnlyPrimaryUserApps: () -> Unit,
    val onOpenProfile: (GroupedApps) -> Unit,
)

@Immutable
data class StatusMeta(
    val label: String,
    val bg: Color,
    val fg: Color
)
