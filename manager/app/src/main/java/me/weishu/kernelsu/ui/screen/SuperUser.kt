package me.weishu.kernelsu.ui.screen

import android.content.Context
import android.content.pm.ApplicationInfo
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.ime
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.edit
import androidx.lifecycle.viewmodel.compose.viewModel
import com.kyant.capsule.ContinuousRoundedRectangle
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.SearchBox
import me.weishu.kernelsu.ui.component.SearchPager
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.viewmodel.GroupedApps
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.DropdownImpl
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import top.yukonga.miuix.kmp.extra.SuperListPopup
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.basic.ArrowRight
import top.yukonga.miuix.kmp.icon.extended.MoreCircle
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun SuperUserPager(
    navigator: Navigator, bottomInnerPadding: Dp
) {
    val viewModel = viewModel<SuperUserViewModel>()
    val uiState by viewModel.uiState.collectAsState()
    val searchStatus = uiState.searchStatus

    val context = LocalContext.current
    var isInitialized by rememberSaveable { mutableStateOf(false) }
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val enableBlur = LocalEnableBlur.current
    val listState = rememberLazyListState()
    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        when {
            !isInitialized || uiState.appList.isEmpty() -> {
                viewModel.setShowSystemApps(prefs.getBoolean("show_system_apps", false))
                viewModel.setShowOnlyPrimaryUserApps(prefs.getBoolean("show_only_primary_user_apps", false))
                viewModel.loadAppList()
                isInitialized = true
            }

            viewModel.isNeedRefresh -> {
                viewModel.loadAppList(resort = false)
            }
        }
    }

    LaunchedEffect(searchStatus.searchText) {
        viewModel.updateSearchText(searchStatus.searchText)
    }

    val scrollBehavior = MiuixScrollBehavior()
    val dynamicTopPadding by remember {
        derivedStateOf { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }

    val hazeState = remember { HazeState() }
    val hazeStyle = if (enableBlur) {
        HazeStyle(
            backgroundColor = colorScheme.surface, tint = HazeTint(colorScheme.surface.copy(0.8f))
        )
    } else {
        HazeStyle.Unspecified
    }

    val isMultiUser = remember(uiState.userIds) {
        uiState.userIds.size > 1
    }

    Scaffold(
        topBar = {
            searchStatus.TopAppBarAnim(hazeState = hazeState, hazeStyle = hazeStyle) {
                TopAppBar(
                    color = if (enableBlur) Color.Transparent else colorScheme.surface,
                    title = stringResource(R.string.superuser),
                    actions = {
                        val showTopPopup = remember { mutableStateOf(false) }
                        SuperListPopup(
                            show = showTopPopup,
                            popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                            alignment = PopupPositionProvider.Align.TopEnd,
                            onDismissRequest = {
                                showTopPopup.value = false
                            }) {
                            val size = if (isMultiUser) 2 else 1
                            ListPopupColumn {
                                DropdownImpl(
                                    text = stringResource(R.string.show_system_apps),
                                    isSelected = uiState.showSystemApps,
                                    optionSize = size,
                                    onSelectedIndexChange = {
                                        val newValue = !uiState.showSystemApps
                                        val job = viewModel.setShowSystemApps(newValue)
                                        prefs.edit {
                                            putBoolean("show_system_apps", newValue)
                                        }
                                        showTopPopup.value = false
                                        scope.launch {
                                            job.join()
                                            listState.animateScrollToItem(0)
                                        }
                                    },
                                    index = 0
                                )
                                if (isMultiUser) {
                                    DropdownImpl(
                                        text = stringResource(R.string.show_only_primary_user_apps),
                                        isSelected = uiState.showOnlyPrimaryUserApps,
                                        optionSize = size,
                                        onSelectedIndexChange = {
                                            val newValue = !uiState.showOnlyPrimaryUserApps
                                            val job = viewModel.setShowOnlyPrimaryUserApps(newValue)
                                            prefs.edit {
                                                putBoolean("show_only_primary_user_apps", newValue)
                                            }
                                            showTopPopup.value = false
                                            scope.launch {
                                                job.join()
                                                listState.animateScrollToItem(0)
                                            }
                                        },
                                        index = 1
                                    )
                                }
                            }
                        }
                        IconButton(
                            modifier = Modifier.padding(end = 16.dp), onClick = {
                                showTopPopup.value = true
                            }, holdDownState = showTopPopup.value
                        ) {
                            Icon(
                                imageVector = MiuixIcons.MoreCircle, tint = colorScheme.onSurface, contentDescription = null
                            )
                        }
                    },
                    scrollBehavior = scrollBehavior
                )
            }
        }, popupHost = {
            val allGroups = uiState.groupedApps
            val matchedByUid = remember(uiState.searchResults) {
                uiState.searchResults.groupBy { it.uid }
            }
            val searchGroups = remember(allGroups, matchedByUid) {
                allGroups.filter { matchedByUid.containsKey(it.uid) }
            }
            val expandedSearchUids = remember { mutableStateOf(setOf<Int>()) }
            LaunchedEffect(matchedByUid) {
                expandedSearchUids.value = searchGroups.filter { it.apps.size > 1 }.map { it.uid }.toSet()
            }
            searchStatus.SearchPager(
                onSearchStatusChange = viewModel::updateSearchStatus,
                defaultResult = {},
                searchBarTopPadding = dynamicTopPadding,
            ) {
                item {
                    Spacer(Modifier.height(6.dp))
                }
                items(searchGroups, key = { it.uid }) { group ->
                    val expanded = expandedSearchUids.value.contains(group.uid)
                    AnimatedVisibility(
                        visible = searchGroups.isNotEmpty(), enter = fadeIn() + expandVertically(), exit = fadeOut() + shrinkVertically()
                    ) {
                        Column {
                            GroupItem(
                                group = group,
                                onToggleExpand = {
                                    if (group.apps.size > 1) {
                                        expandedSearchUids.value =
                                            if (expanded) expandedSearchUids.value - group.uid else expandedSearchUids.value + group.uid
                                    }
                                },
                            ) {
                                navigator.push(Route.AppProfile(group.uid, group.primary.packageName))
                                viewModel.markNeedRefresh()
                            }
                            AnimatedVisibility(
                                visible = expanded && group.apps.size > 1,
                                enter = expandVertically() + fadeIn(),
                                exit = shrinkVertically() + fadeOut()
                            ) {
                                Column {
                                    val matchedApps = matchedByUid[group.uid] ?: emptyList()
                                    matchedApps.forEach { app -> SimpleAppItem(app) }
                                    Spacer(Modifier.height(6.dp))
                                }
                            }
                        }
                    }
                }
                item {
                    val imeBottomPadding = WindowInsets.ime.asPaddingValues().calculateBottomPadding()
                    Spacer(Modifier.height(maxOf(bottomInnerPadding, imeBottomPadding)))
                }
            }
        }, contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val layoutDirection = LocalLayoutDirection.current
        searchStatus.SearchBox(
            onSearchStatusChange = viewModel::updateSearchStatus,
            searchBarTopPadding = dynamicTopPadding, contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding(),
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection)
            ), hazeState = hazeState, hazeStyle = hazeStyle
        ) { boxHeight ->
            val pullToRefreshState = rememberPullToRefreshState()
            val refreshTexts = listOf(
                stringResource(R.string.refresh_pulling),
                stringResource(R.string.refresh_release),
                stringResource(R.string.refresh_refresh),
                stringResource(R.string.refresh_complete),
            )
            if (uiState.appList.isEmpty() && uiState.isRefreshing) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(
                            top = innerPadding.calculateTopPadding(),
                            start = innerPadding.calculateStartPadding(layoutDirection),
                            end = innerPadding.calculateEndPadding(layoutDirection),
                            bottom = bottomInnerPadding
                        ), contentAlignment = Alignment.Center
                ) {
                    InfiniteProgressIndicator()
                }
            } else {
                val allGroups = uiState.groupedApps
                val visibleUidSet = remember(uiState.appList) { uiState.appList.map { it.uid }.toSet() }
                val expandedUids = remember { mutableStateOf(setOf<Int>()) }
                PullToRefresh(
                    isRefreshing = uiState.isRefreshing,
                    pullToRefreshState = pullToRefreshState,
                    onRefresh = {
                        val job = viewModel.loadAppList(force = true)
                        scope.launch {
                            job.join()
                            delay(10)
                            listState.animateScrollToItem(0)
                        }
                    },
                    refreshTexts = refreshTexts,
                    contentPadding = PaddingValues(
                        top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection)
                    ),
                ) {
                    LazyColumn(
                        state = listState,
                        modifier = Modifier.fillMaxHeight().scrollEndHaptic().overScrollVertical()
                            .nestedScroll(scrollBehavior.nestedScrollConnection)
                            .let { if (enableBlur) it.hazeSource(state = hazeState) else it },
                        contentPadding = PaddingValues(
                            top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                            start = innerPadding.calculateStartPadding(layoutDirection),
                            end = innerPadding.calculateEndPadding(layoutDirection)
                        ),
                        overscrollEffect = null,
                    ) {
                        items(allGroups, key = { it.uid }) { group ->
                            val expanded = expandedUids.value.contains(group.uid)
                            val isVisible = visibleUidSet.contains(group.uid)
                            AnimatedVisibility(
                                visible = isVisible, enter = expandVertically() + fadeIn(), exit = shrinkVertically() + fadeOut()
                            ) {
                                Column {
                                    GroupItem(
                                        group = group, onToggleExpand = {
                                            if (group.apps.size > 1) {
                                                expandedUids.value =
                                                    if (expanded) expandedUids.value - group.uid else expandedUids.value + group.uid
                                            }
                                        }) {
                                        navigator.push(Route.AppProfile(group.uid, group.primary.packageName))
                                        viewModel.markNeedRefresh()
                                    }
                                    AnimatedVisibility(
                                        visible = expanded && group.apps.size > 1,
                                        enter = expandVertically() + fadeIn(),
                                        exit = shrinkVertically() + fadeOut()
                                    ) {
                                        Column {
                                            group.apps.forEach { app ->
                                                SimpleAppItem(app)
                                            }
                                            Spacer(Modifier.height(6.dp))
                                        }
                                    }
                                }
                            }
                        }
                        item {
                            Spacer(Modifier.height(bottomInnerPadding))
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SimpleAppItem(
    app: SuperUserViewModel.AppInfo,
) {
    Row {
        Box(
            modifier = Modifier
                .padding(start = 12.dp)
                .width(6.dp)
                .height(24.dp)
                .align(Alignment.CenterVertically)
                .clip(ContinuousRoundedRectangle(16.dp))
                .background(colorScheme.primaryContainer)
        )
        Card(
            modifier = Modifier.padding(start = 6.dp, end = 12.dp, bottom = 6.dp)
        ) {
            BasicComponent(
                title = app.label, summary = app.packageName, startAction = {
                    AppIconImage(
                        packageInfo = app.packageInfo, label = app.label, modifier = Modifier
                            .padding(end = 9.dp)
                            .size(40.dp)
                    )
                }, insideMargin = PaddingValues(horizontal = 9.dp)
            )
        }
    }
}

@Composable
private fun GroupItem(
    group: GroupedApps,
    onToggleExpand: () -> Unit,
    onClickPrimary: () -> Unit,
) {
    val isInDarkTheme = isInDarkTheme()
    val bg = colorScheme.secondaryContainer.copy(alpha = 0.8f)
    val rootBg = colorScheme.tertiaryContainer.copy(alpha = 0.6f)
    val unmountBg = if (isInDarkTheme) Color.White.copy(alpha = 0.4f) else Color.Black.copy(alpha = 0.3f)
    val fg = colorScheme.onSecondaryContainer
    val rootFg = colorScheme.onTertiaryContainer.copy(alpha = 0.8f)
    val unmountFg = if (isInDarkTheme) Color.Black.copy(alpha = 0.4f) else Color.White.copy(alpha = 0.8f)

    val userId = group.uid / 100000
    val packageInfo = group.primary.packageInfo
    val applicationInfo = packageInfo.applicationInfo
    val hasSharedUserId = !packageInfo.sharedUserId.isNullOrEmpty()
    val isSystemApp =
        applicationInfo?.flags?.and(ApplicationInfo.FLAG_SYSTEM) != 0 || applicationInfo.flags.and(ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0
    val tags = buildList {
        if (group.anyAllowSu) add(StatusMeta("ROOT", rootBg, rootFg))
        if (group.shouldUmount) add(StatusMeta("UMOUNT", unmountBg, unmountFg))
        if (group.anyCustom) add(StatusMeta("CUSTOM", bg, fg))
        if (userId != 0) add(StatusMeta("USER $userId", bg, fg))
        if (isSystemApp) add(StatusMeta("SYSTEM", bg, fg))
        if (hasSharedUserId) add(StatusMeta("SHARED UID", bg, fg))
    }
    Card(
        modifier = Modifier
            .padding(horizontal = 12.dp)
            .padding(bottom = 12.dp),
        onClick = onClickPrimary,
        onLongPress = if (group.apps.size > 1) onToggleExpand else null,
        pressFeedbackType = PressFeedbackType.Sink,
        showIndication = true,
        insideMargin = PaddingValues(vertical = 8.dp, horizontal = 16.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically
        ) {
            AppIconImage(
                packageInfo = group.primary.packageInfo, label = group.primary.label, modifier = Modifier
                    .padding(end = 14.dp)
                    .size(48.dp)
            )
            Column(
                modifier = Modifier.weight(1f),
            ) {
                Text(
                    text = group.ownerName ?: group.primary.label,
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurface,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    text = if (group.apps.size > 1) {
                        stringResource(R.string.group_contains_apps, group.apps.size)
                    } else {
                        group.primary.packageName
                    },
                    fontSize = 12.sp,
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurfaceVariantSummary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                FlowRow(
                    modifier = Modifier.padding(top = 3.dp, bottom = 3.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    tags.forEach { tag ->
                        StatusTag(
                            label = tag.label, backgroundColor = tag.bg, contentColor = tag.fg
                        )
                    }
                }
            }
            val layoutDirection = LocalLayoutDirection.current
            Image(
                modifier = Modifier
                    .graphicsLayer {
                        if (layoutDirection == LayoutDirection.Rtl) scaleX = -1f
                    }
                    .padding(start = 8.dp)
                    .size(width = 10.dp, height = 16.dp),
                imageVector = MiuixIcons.Basic.ArrowRight,
                contentDescription = null,
                colorFilter = ColorFilter.tint(colorScheme.onSurfaceVariantActions),
            )
        }
    }
}

@Composable
fun StatusTag(
    label: String, backgroundColor: Color, contentColor: Color
) {
    Box(
        modifier = Modifier.background(
            color = backgroundColor, shape = ContinuousRoundedRectangle(6.dp)
        )
    ) {
        Text(
            modifier = Modifier.padding(horizontal = 4.dp, vertical = 2.dp),
            text = label,
            color = contentColor,
            fontSize = 9.sp,
            fontWeight = FontWeight(750),
            maxLines = 1,
            softWrap = false
        )
    }
}

@Immutable
private data class StatusMeta(
    val label: String, val bg: Color, val fg: Color
)
