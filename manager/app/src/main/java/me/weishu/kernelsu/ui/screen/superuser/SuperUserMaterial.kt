package me.weishu.kernelsu.ui.screen.superuser

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Article
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme.colorScheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.material3.surfaceColorAtElevation
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.AppInfo
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.material.SearchAppBar
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedItem
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.util.ownerNameForUid

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun SuperUserPagerMaterial(
    uiState: SuperUserUiState,
    actions: SuperUserActions,
    bottomInnerPadding: Dp,
) {
    val scope = rememberCoroutineScope()
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val listState = rememberLazyListState()
    val searchListState = rememberLazyListState()
    val pullToRefreshState = rememberPullToRefreshState()

    var localSearchText by remember { mutableStateOf(uiState.searchStatus.searchText) }
    LaunchedEffect(uiState.searchStatus.searchText) {
        localSearchText = uiState.searchStatus.searchText
    }

    val haptic = LocalHapticFeedback.current

    Scaffold(
        topBar = {
            SearchAppBar(
                title = { Text(stringResource(R.string.superuser)) },
                searchText = localSearchText,
                onSearchTextChange = {
                    localSearchText = it
                    actions.onSearchTextChange(it)
                    scope.launch { listState.scrollToItem(0) }
                },
                onClearClick = {
                    localSearchText = ""
                    actions.onClearSearch()
                },
                navigationIcon = {
                    IconButton(onClick = actions.onOpenSulog) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Outlined.Article,
                            contentDescription = stringResource(R.string.settings_sulog)
                        )
                    }
                },
                actions = {
                    var showDropdown by remember { mutableStateOf(false) }

                    IconButton(onClick = { showDropdown = true }) {
                        Icon(
                            imageVector = Icons.Filled.MoreVert,
                            contentDescription = stringResource(id = R.string.settings)
                        )

                        DropdownMenu(
                            expanded = showDropdown,
                            onDismissRequest = { showDropdown = false }
                        ) {
                            DropdownMenuItem(
                                text = { Text(stringResource(R.string.show_system_apps)) },
                                trailingIcon = { Checkbox(uiState.showSystemApps, null) },
                                onClick = {
                                    haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                    actions.onToggleShowSystemApps()
                                    showDropdown = false
                                }
                            )
                            if (uiState.userIds.size > 1) {
                                DropdownMenuItem(
                                    text = { Text(stringResource(R.string.show_only_primary_user_apps)) },
                                    trailingIcon = { Checkbox(uiState.showOnlyPrimaryUserApps, null) },
                                    onClick = {
                                        haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                        actions.onToggleShowOnlyPrimaryUserApps()
                                        showDropdown = false
                                    }
                                )
                            }
                        }
                    }
                },
                scrollBehavior = scrollBehavior,
                defaultContent = { bottomPadding, closeSearch ->
                    LaunchedEffect(localSearchText) {
                        searchListState.scrollToItem(0)
                    }
                    LazyColumn(
                        state = searchListState,
                        modifier = Modifier
                            .fillMaxSize()
                            .nestedScroll(scrollBehavior.nestedScrollConnection),
                        verticalArrangement = Arrangement.spacedBy(2.dp),
                        contentPadding = PaddingValues(
                            start = 16.dp,
                            end = 16.dp,
                            top = 8.dp,
                            bottom = 16.dp + bottomPadding
                        ),
                    ) {
                        if (uiState.recentlyInstalledResults.isNotEmpty()) {
                            item {
                                SegmentedColumn(
                                    title = stringResource(R.string.recently_installed),
                                    content = uiState.recentlyInstalledResults.map { group ->
                                        @Composable {
                                            SearchGroupItem(
                                                group = group,
                                                closeSearch = closeSearch,
                                                onOpenProfile = actions.onOpenProfile,
                                            )
                                        }
                                    }
                                )
                            }
                        }
                    }
                },
                searchContent = { bottomPadding, closeSearch ->
                    LaunchedEffect(localSearchText) {
                        searchListState.scrollToItem(0)
                    }
                    LazyColumn(
                        state = searchListState,
                        modifier = Modifier
                            .fillMaxSize()
                            .nestedScroll(scrollBehavior.nestedScrollConnection),
                        verticalArrangement = Arrangement.spacedBy(2.dp),
                        contentPadding = PaddingValues(
                            start = 16.dp,
                            end = 16.dp,
                            top = 8.dp,
                            bottom = 16.dp + bottomPadding
                        ),
                    ) {
                        itemsIndexed(uiState.searchResults, key = { _, item -> item.uid }) { index, group ->
                            SegmentedItem(index = index, count = uiState.searchResults.size) {
                                SearchGroupItem(
                                    group = group,
                                    closeSearch = closeSearch,
                                    onOpenProfile = actions.onOpenProfile,
                                )
                            }
                        }
                    }
                }
            )
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        PullToRefreshBox(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding),
            isRefreshing = uiState.isRefreshing,
            onRefresh = {
                haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                actions.onRefresh()
            },
            state = pullToRefreshState,
            indicator = {
                PullToRefreshDefaults.LoadingIndicator(
                    modifier = Modifier.align(Alignment.TopCenter),
                    isRefreshing = uiState.isRefreshing,
                    state = pullToRefreshState,
                )
            },
        ) {
            val expandedSearchUids = remember { mutableStateOf(setOf<Int>()) }

            LazyColumn(
                state = listState,
                modifier = Modifier
                    .fillMaxSize()
                    .nestedScroll(scrollBehavior.nestedScrollConnection),
                verticalArrangement = Arrangement.spacedBy(2.dp),
                contentPadding = PaddingValues(
                    start = 16.dp,
                    end = 16.dp,
                    top = 8.dp,
                    bottom = 16.dp + bottomInnerPadding
                ),
            ) {
                itemsIndexed(uiState.groupedApps, key = { _, item -> item.uid }) { index, group ->
                    val expanded = expandedSearchUids.value.contains(group.uid)
                    val onToggleExpand = {
                        if (group.apps.size > 1) {
                            expandedSearchUids.value = if (expandedSearchUids.value.contains(group.uid)) {
                                expandedSearchUids.value - group.uid
                            } else {
                                expandedSearchUids.value + group.uid
                            }
                        }
                    }
                    SegmentedItem(index = index, count = uiState.groupedApps.size) {
                        Column {
                            GroupItem(
                                group = group,
                                selected = expanded,
                                onToggleExpand = onToggleExpand,
                            ) {
                                actions.onOpenProfile(group)
                            }
                            AnimatedVisibility(
                                visible = expanded && group.apps.size > 1,
                                enter = expandVertically() + fadeIn(),
                                exit = shrinkVertically() + fadeOut()
                            ) {
                                Column {
                                    group.apps.forEach { app ->
                                        SimpleAppItem(app = app) {
                                            actions.onOpenProfile(group)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SearchGroupItem(
    group: GroupedApps,
    closeSearch: () -> Unit,
    onOpenProfile: (GroupedApps) -> Unit,
) {
    Column {
        GroupItem(
            group = group,
            selected = false,
            onToggleExpand = {},
        ) {
            closeSearch()
            onOpenProfile(group)
        }
        AnimatedVisibility(
            visible = group.apps.size > 1,
            enter = expandVertically() + fadeIn(),
            exit = shrinkVertically() + fadeOut()
        ) {
            Column {
                group.apps.forEach { app ->
                    SimpleAppItem(
                        app = app,
                        matched = group.matchedPackageNames.contains(app.packageName),
                    ) {
                        closeSearch()
                        onOpenProfile(group)
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun SimpleAppItem(
    app: AppInfo,
    matched: Boolean = false,
    onNavigate: () -> Unit,
) {
    ListItem(
        onClick = onNavigate,
        modifier = Modifier.padding(horizontal = 4.dp),
        shapes = ListItemDefaults.shapes(shape = RoundedCornerShape(0.dp)),
        colors = ListItemDefaults.colors(
            containerColor = if (matched) {
                colorScheme.secondaryContainer
            } else {
                colorScheme.surfaceColorAtElevation(3.dp)
            }
        ),
        content = { Text(app.label, overflow = TextOverflow.Ellipsis, maxLines = 1) },
        supportingContent = { Text(app.packageName, overflow = TextOverflow.Ellipsis, maxLines = 1) },
        leadingContent = {
            AppIconImage(
                packageInfo = app.packageInfo,
                label = app.label,
                modifier = Modifier
                    .size(40.dp)
                    .padding(start = 4.dp)
            )
        },
        trailingContent = {
            Icon(
                Icons.Filled.Remove,
                contentDescription = null,
                modifier = Modifier.padding(end = 4.dp)
            )
        }
    )
}

@Composable
private fun GroupItem(
    group: GroupedApps,
    selected: Boolean,
    onToggleExpand: () -> Unit,
    onClickPrimary: () -> Unit,
) {
    val bg = colorScheme.primary
    val fg = colorScheme.onPrimary
    val umountBg = colorScheme.secondary
    val umountFg = colorScheme.onSecondary
    val customBg = colorScheme.secondaryContainer
    val customFg = colorScheme.onSecondaryContainer
    val otherBg = colorScheme.tertiary
    val otherFg = colorScheme.onTertiary

    val userId = group.uid / 100000
    val tags = remember(group.anyAllowSu, group.shouldUmount, group.anyCustom, userId) {
        buildList {
            if (group.anyAllowSu) add(StatusMeta("ROOT", bg, fg))
            if (group.shouldUmount) add(StatusMeta("UMOUNT", umountBg, umountFg))
            if (group.anyCustom) add(StatusMeta("CUSTOM", customBg, customFg))
            if (userId != 0) add(StatusMeta("USER $userId", otherBg, otherFg))
        }
    }
    val summaryText = if (group.apps.size > 1) {
        stringResource(R.string.group_contains_apps, group.apps.size)
    } else {
        group.primary.packageName
    }
    SegmentedListItem(
        selected = selected,
        onClick = onClickPrimary,
        onLongClick = if (group.apps.size > 1) onToggleExpand else null,
        headlineContent = {
            Text(
                text = if (group.apps.size > 1) ownerNameForUid(group.uid) else group.primary.label,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1
            )
        },
        supportingContent = {
            Text(
                text = summaryText,
                color = colorScheme.outline,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1
            )
        },
        trailingContent = {
            if (tags.isNotEmpty()) {
                Column(
                    horizontalAlignment = Alignment.End,
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    tags.forEach { tag ->
                        StatusTag(
                            label = tag.label,
                            backgroundColor = tag.bg,
                            contentColor = tag.fg
                        )
                    }
                }
            }
        },
        leadingContent = {
            AppIconImage(
                packageInfo = group.primary.packageInfo,
                label = group.primary.label,
                modifier = Modifier.size(48.dp)
            )
        },
    )
}
