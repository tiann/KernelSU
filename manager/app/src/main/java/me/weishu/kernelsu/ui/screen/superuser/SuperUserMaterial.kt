package me.weishu.kernelsu.ui.screen.superuser

import android.content.Context
import android.content.pm.ApplicationInfo
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.LinearOutSlowInEasing
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
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
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.pullToRefresh
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.material3.surfaceColorAtElevation
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.core.content.edit
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.AppInfo
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.material.SearchAppBar
import me.weishu.kernelsu.ui.component.material.SegmentedLazyColumn
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.viewmodel.GroupedApps
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun SuperUserPagerMaterial(navigator: Navigator, bottomInnerPadding: Dp) {
    val viewModel = viewModel<SuperUserViewModel>()
    val uiState by viewModel.uiState.collectAsState()
    val scope = rememberCoroutineScope()

    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val listState = rememberLazyListState()
    val searchListState = rememberLazyListState()
    val pullToRefreshState = rememberPullToRefreshState()

    val onRefresh: () -> Unit = {
        scope.launch {
            viewModel.loadAppList(true)
        }
    }

    val scaleFraction = {
        if (uiState.isRefreshing) 1f
        else LinearOutSlowInEasing.transform(pullToRefreshState.distanceFraction).coerceIn(0f, 1f)
    }

    LaunchedEffect(key1 = navigator) {
        when {
            uiState.appList.isEmpty() -> {
                viewModel.setShowSystemApps(prefs.getBoolean("show_system_apps", false))
                viewModel.setShowOnlyPrimaryUserApps(prefs.getBoolean("show_only_primary_user_apps", false))
                viewModel.loadAppList()
            }
            viewModel.isNeedRefresh -> {
                viewModel.loadAppList(resort = false)
            }
        }
    }

    var localSearchText by remember { mutableStateOf(uiState.searchStatus.searchText) }
    LaunchedEffect(uiState.searchStatus.searchText) {
        localSearchText = uiState.searchStatus.searchText
    }

    val isMultiUser = remember(uiState.userIds) {
        uiState.userIds.size > 1
    }

    Scaffold(
        modifier = Modifier
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .pullToRefresh(
                state = pullToRefreshState,
                isRefreshing = uiState.isRefreshing,
                onRefresh = onRefresh,
            ),
        topBar = {
            SearchAppBar(
                title = { Text(stringResource(R.string.superuser)) },
                searchText = localSearchText,
                onSearchTextChange = {
                    localSearchText = it
                    scope.launch { viewModel.updateSearchText(it) }
                    scope.launch { listState.scrollToItem(0) }
                },
                onClearClick = {
                    localSearchText = ""
                    scope.launch { viewModel.updateSearchText("") }
                },
                actions = {
                    var showDropdown by remember { mutableStateOf(false) }

                    IconButton(
                        onClick = { showDropdown = true },
                    ) {
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
                                    val newValue = !uiState.showSystemApps
                                    viewModel.setShowSystemApps(newValue)
                                    prefs.edit {
                                        putBoolean("show_system_apps", newValue)
                                    }
                                    showDropdown = false
                                }
                            )
                            if (isMultiUser) DropdownMenuItem(
                                text = { Text(stringResource(R.string.show_only_primary_user_apps)) },
                                trailingIcon = { Checkbox(uiState.showOnlyPrimaryUserApps, null) },
                                onClick = {
                                    val newValue = !uiState.showOnlyPrimaryUserApps
                                    viewModel.setShowOnlyPrimaryUserApps(newValue)
                                    prefs.edit {
                                        putBoolean("show_only_primary_user_apps", newValue)
                                    }
                                    showDropdown = false
                                }
                            )
                        }
                    }
                },
                scrollBehavior = scrollBehavior,
                searchContent = { closeSearch ->
                    LaunchedEffect(localSearchText) {
                        searchListState.scrollToItem(0)
                    }
                    val allGroups = uiState.groupedApps
                    val searchGroups = remember(uiState.searchResults) {
                        uiState.searchResults.groupBy { it.uid }.keys
                    }
                    val visibleSearchGroups = remember(allGroups, searchGroups) {
                        allGroups.filter { it.uid in searchGroups }
                    }

                    SegmentedLazyColumn(
                        state = searchListState,
                        modifier = Modifier
                            .fillMaxSize()
                            .nestedScroll(scrollBehavior.nestedScrollConnection),
                        contentPadding = PaddingValues(
                            start = 16.dp,
                            end = 16.dp,
                            top = 8.dp,
                            bottom = 16.dp + bottomInnerPadding
                        ),
                        key = { it.uid },
                        items = visibleSearchGroups,
                    ) { group ->
                        Column {
                            GroupItem(
                                group = group,
                                selected = false,
                                onToggleExpand = {},
                            ) {
                                closeSearch()
                                navigator.push(Route.AppProfile(group.uid, group.primary.packageName))
                            }
                            AnimatedVisibility(
                                visible = group.apps.size > 1,
                                enter = expandVertically() + fadeIn(),
                                exit = shrinkVertically() + fadeOut()
                            ) {
                                Column {
                                    val filteredApps = group.apps.filter { it in uiState.searchResults }
                                    filteredApps.forEach { app ->
                                        SimpleAppItem(app) {
                                            closeSearch()
                                            navigator.push(Route.AppProfile(group.uid, group.primary.packageName))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            )
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        Box(modifier = Modifier.padding(innerPadding)) {
            val allGroups = uiState.groupedApps
            val visibleUids = remember(uiState.appList) { uiState.appList.map { it.uid }.toSet() }
            val expandedSearchUids = remember { mutableStateOf(setOf<Int>()) }
            val visibleGroups = remember(allGroups, visibleUids) {
                allGroups.filter { it.uid in visibleUids }
            }

            SegmentedLazyColumn(
                state = listState,
                modifier = Modifier
                    .fillMaxSize()
                    .nestedScroll(scrollBehavior.nestedScrollConnection),
                contentPadding = PaddingValues(
                    start = 16.dp,
                    end = 16.dp,
                    top = 8.dp,
                    bottom = 16.dp + bottomInnerPadding
                ),
                key = { it.uid },
                items = visibleGroups,
            ) { group ->
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
                Column {
                    GroupItem(
                        group = group,
                        selected = expanded,
                        onToggleExpand = onToggleExpand,
                    ) {
                        navigator.push(Route.AppProfile(group.uid, group.primary.packageName))
                    }
                    AnimatedVisibility(
                        visible = expanded && group.apps.size > 1,
                        enter = expandVertically() + fadeIn(),
                        exit = shrinkVertically() + fadeOut()
                    ) {
                        Column {
                            val filteredApps = group.apps.filter { it in uiState.appList }
                            filteredApps.forEach { app ->
                                SimpleAppItem(app) {
                                    navigator.push(Route.AppProfile(group.uid, group.primary.packageName))
                                }
                            }
                        }
                    }
                }
            }
            Box(
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .graphicsLayer {
                        scaleX = scaleFraction()
                        scaleY = scaleFraction()
                    }
            ) {
                PullToRefreshDefaults.LoadingIndicator(state = pullToRefreshState, isRefreshing = uiState.isRefreshing)
            }
        }
    }
}

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun SimpleAppItem(
    app: AppInfo,
    onNavigate: () -> Unit,
) {
    ListItem(
        onClick = onNavigate,
        modifier = Modifier.padding(horizontal = 4.dp),
        shapes = ListItemDefaults.shapes(shape = RoundedCornerShape(0.dp)),
        colors = ListItemDefaults.colors(containerColor = MaterialTheme.colorScheme.surfaceColorAtElevation(3.dp)),
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
            Column {
                Text(
                    text = summaryText,
                    color = MaterialTheme.colorScheme.outline,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1
                )
                FlowRow {
                    val userId = group.uid / 100000
                    val packageInfo = group.primary.packageInfo
                    val applicationInfo = packageInfo.applicationInfo

                    if (group.anyAllowSu) {
                        StatusTag(
                            label = "ROOT",
                            modifier = Modifier.padding(top = 4.dp),
                            contentColor = MaterialTheme.colorScheme.onPrimary,
                            backgroundColor = MaterialTheme.colorScheme.primary
                        )
                    } else if (group.shouldUmount) {
                        StatusTag(
                            label = "UMOUNT",
                            modifier = Modifier.padding(top = 4.dp),
                            contentColor = MaterialTheme.colorScheme.onSecondary,
                            backgroundColor = MaterialTheme.colorScheme.secondary
                        )
                    }
                    if (group.anyCustom) {
                        StatusTag(
                            label = "CUSTOM",
                            modifier = Modifier.padding(top = 4.dp),
                            contentColor = MaterialTheme.colorScheme.onSecondaryContainer,
                            backgroundColor = MaterialTheme.colorScheme.secondaryContainer
                        )
                    }
                    if (userId != 0) {
                        StatusTag(
                            label = "USER $userId",
                            modifier = Modifier.padding(top = 4.dp),
                            contentColor = MaterialTheme.colorScheme.onTertiary,
                            backgroundColor = MaterialTheme.colorScheme.tertiary
                        )
                    }
                    if (applicationInfo?.flags?.and(ApplicationInfo.FLAG_SYSTEM) != 0
                        || applicationInfo.flags.and(ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0) {
                        StatusTag(
                            label = "SYSTEM",
                            modifier = Modifier.padding(top = 4.dp),
                            contentColor = MaterialTheme.colorScheme.onTertiary,
                            backgroundColor = MaterialTheme.colorScheme.tertiary
                        )
                    }
                    if (!packageInfo.sharedUserId.isNullOrEmpty()) {
                        StatusTag(
                            label = "SHARED UID",
                            modifier = Modifier.padding(top = 4.dp),
                            contentColor = MaterialTheme.colorScheme.onTertiary,
                            backgroundColor = MaterialTheme.colorScheme.tertiary
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
