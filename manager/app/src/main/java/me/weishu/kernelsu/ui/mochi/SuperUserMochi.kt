package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.toImmutableList
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.screen.superuser.GroupedApps
import me.weishu.kernelsu.ui.screen.superuser.SuperUserActions
import me.weishu.kernelsu.ui.screen.superuser.SuperUserUiState
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.viewmodel.AppSortType
import ui.feice.mochi.basic.MochiBarIconButton
import ui.feice.mochi.basic.MochiBarGroupDropdownItem
import ui.feice.mochi.basic.MochiBarGroupItem
import ui.feice.mochi.basic.MochiBarGroup
import ui.feice.mochi.basic.MochiBarIconDropdown
import ui.feice.mochi.basic.MochiEmptyState
import ui.feice.mochi.basic.MochiItem
import ui.feice.mochi.basic.MochiSearchBar
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.LocalMochiListSupportingTextGap
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.list.mochiListSection
import ui.feice.mochi.list.rows
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold
import ui.feice.mochi.theme.MochiTheme

private val AppIconSize = 36.dp

/** Triggers a refresh and waits until [refreshing] goes true (up to 400ms) and back to false. */
internal suspend fun awaitRefresh(start: () -> Unit, refreshing: State<Boolean>) {
    start()
    var waited = 0
    while (!refreshing.value && waited < 400) {
        delay(50)
        waited += 50
    }
    snapshotFlow { refreshing.value }.first { !it }
}

internal fun checkIcon(on: Boolean): (@Composable (androidx.compose.ui.graphics.Color) -> Unit)? =
    if (on) {
        { tint -> MochiIonicon("checkmark", tint = tint) }
    } else {
        null
    }

@Composable
fun SuperUserPagerMochi(uiState: SuperUserUiState, actions: SuperUserActions) {
    val refreshing = rememberUpdatedState(uiState.isRefreshing)
    val query = uiState.searchStatus.searchText
    val searching = query.isNotBlank()
    val apps = if (searching) uiState.searchResults else uiState.groupedApps

    val sortTypes = listOf(
        AppSortType.NAME to R.string.sort_by_name,
        AppSortType.PACKAGE_NAME to R.string.sort_by_package_name,
        AppSortType.INSTALL_TIME to R.string.sort_by_install_time,
        AppSortType.UPDATE_TIME to R.string.sort_by_update_time,
    )
    val sortLabels = sortTypes.map { stringResource(it.second) }
    val reverseLabel = stringResource(R.string.sort_reverse)
    val systemLabel = stringResource(R.string.show_system_apps)
    val primaryLabel = stringResource(R.string.show_only_primary_user_apps)
    val multiUser = uiState.userIds.size > 1
    var menu by remember { mutableStateOf(false) }
    val sulog = stringResource(R.string.settings_sulog)
    val more = stringResource(R.string.mochi_more)
    val rootHeader = stringResource(R.string.mochi_su_section_root)
    val customHeader = stringResource(R.string.mochi_su_section_custom)
    val othersHeader = stringResource(R.string.mochi_su_section_others)

    MochiLazyLargeTitleScaffold(
        title = stringResource(R.string.superuser),
        actions = {
          MochiBarGroup(navId = "su.actions") {
            MochiBarGroupItem(
                onClick = actions.onOpenSulog,
                modifier = Modifier.semantics { contentDescription = sulog },
                navId = "su.log",
                contentKey = "reader",
            ) { MochiIonicon("reader") }
            val config = uiState.sortConfig
            val items = buildList {
                sortTypes.forEachIndexed { i, (type, _) -> add(MochiItem(sortLabels[i], icon = checkIcon(config.sortType == type))) }
                add(MochiItem(reverseLabel, icon = checkIcon(config.reversed)))
                add(MochiItem(systemLabel, icon = checkIcon(uiState.showSystemApps)))
                if (multiUser) add(MochiItem(primaryLabel, icon = checkIcon(uiState.showOnlyPrimaryUserApps)))
            }.toImmutableList()
            MochiBarGroupDropdownItem(
                expanded = menu,
                onExpandedChange = { menu = it },
                items = items,
                onItemClick = { index ->
                    when {
                        index < sortTypes.size -> actions.onUpdateSortConfig(config.withType(sortTypes[index].first))
                        index == sortTypes.size -> actions.onUpdateSortConfig(config.toggleReversed())
                        index == sortTypes.size + 1 -> actions.onToggleShowSystemApps()
                        else -> actions.onToggleShowOnlyPrimaryUserApps()
                    }
                },
                modifier = Modifier.semantics { contentDescription = more },
                navId = "su.more",
                contentKey = "filter",
            ) { MochiIonicon("filter") }
          }
        },
        searchBar = {
            MochiSearchBar(
                query = query,
                onQueryChange = actions.onSearchTextChange,
                placeholder = stringResource(R.string.mochi_search_apps),
                onCancel = actions.onClearSearch,
            )
        },
        onRefresh = { awaitRefresh(actions.onRefresh, refreshing) },
    ) {
        if (searching && apps.isEmpty()) {
            item(key = "empty") {
                MochiEmptyState(stringResource(R.string.mochi_no_results), icon = { MochiIonicon("search", size = 44.dp) })
            }
        } else if (searching) {
            mochiListSection(key = "results") {
                rows(apps, key = { it.uid }) { group -> AppGroupRow(group) { actions.onOpenProfile(group) } }
            }
        } else {
            // Groups: root granted, custom profile, others. Tags implied by the group are not drawn again.
            val rooted = apps.filter { it.anyAllowSu }
            val custom = apps.filter { !it.anyAllowSu && it.anyCustom }
            val others = apps.filter { !it.anyAllowSu && !it.anyCustom }
            if (rooted.isNotEmpty()) {
                mochiListSection(key = "rooted", header = rootHeader) {
                    rows(rooted, key = { it.uid }) { group ->
                        AppGroupRow(group, showRoot = false) { actions.onOpenProfile(group) }
                    }
                }
            }
            if (custom.isNotEmpty()) {
                mochiListSection(key = "custom", header = customHeader) {
                    rows(custom, key = { it.uid }) { group ->
                        AppGroupRow(group, showCustom = false) { actions.onOpenProfile(group) }
                    }
                }
            }
            if (others.isNotEmpty()) {
                mochiListSection(key = "others", header = othersHeader) {
                    // Without a custom profile umount is just the global default, so no tag
                    rows(others, key = { it.uid }) { group ->
                        AppGroupRow(group, showUmount = false) { actions.onOpenProfile(group) }
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
internal fun AppGroupRow(
    group: GroupedApps,
    showRoot: Boolean = true,
    showCustom: Boolean = true,
    showUmount: Boolean = true,
    onClick: () -> Unit,
) {
    val cs = MochiTheme.colorScheme
    val userId = group.uid / 100000
    val shared = group.apps.size > 1
    val root = showRoot && group.anyAllowSu
    val umount = showUmount && group.shouldUmount
    val customTag = showCustom && group.anyCustom
    val hasTags = root || umount || customTag || userId != 0
    CompositionLocalProvider(LocalMochiListSupportingTextGap provides 2.dp) {
        MochiListRow(
            if (shared) ownerNameForUid(group.uid) else group.primary.label,
            subtitle = if (shared) stringResource(R.string.group_contains_apps, group.apps.size) else group.primary.displayIdentifier,
            leading = { AppIconImage(packageInfo = group.primary.packageInfo, label = group.primary.label, modifier = Modifier.size(AppIconSize)) },
            leadingSize = AppIconSize,
            trailing = if (hasTags) {
                {
                    FlowRow(
                        horizontalArrangement = Arrangement.spacedBy(4.dp, Alignment.End),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                        maxItemsInEachRow = 2,
                    ) {
                        if (root) MochiListStatusChip("ROOT")
                        if (umount) MochiListStatusChip("UMOUNT", containerColor = cs.secondaryContainer, contentColor = cs.onSecondaryContainer)
                        if (customTag) MochiListStatusChip("CUSTOM", containerColor = cs.tertiaryContainer, contentColor = cs.onTertiaryContainer)
                        if (userId != 0) MochiListStatusChip("USER $userId", containerColor = cs.surfaceVariant, contentColor = cs.onSurfaceVariant)
                    }
                }
            } else null,
            chevron = true,
            onClick = onClick,
        )
    }
}
