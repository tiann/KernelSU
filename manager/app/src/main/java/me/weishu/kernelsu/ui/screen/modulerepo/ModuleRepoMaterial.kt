package me.weishu.kernelsu.ui.screen.modulerepo

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyItemScope
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Sort
import androidx.compose.material.icons.automirrored.outlined.ChromeReaderMode
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.outlined.Download
import androidx.compose.material.icons.outlined.InstallMobile
import androidx.compose.material.icons.outlined.Link
import androidx.compose.material.icons.outlined.Verified
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularWavyProgressIndicator
import androidx.compose.material3.DropdownMenuGroup
import androidx.compose.material3.DropdownMenuPopup
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.LoadingIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuDefaults
import androidx.compose.material3.SelectableDropdownMenuItem
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.RepoModule
import me.weishu.kernelsu.data.model.RepoRelease
import me.weishu.kernelsu.ui.component.ScrollToTopOnChange
import me.weishu.kernelsu.ui.component.dialog.ConfirmDialogHandle
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.markdown.GithubMarkdown
import me.weishu.kernelsu.ui.component.material.ExpressiveScaffold
import me.weishu.kernelsu.ui.component.material.ExpressiveTabRow
import me.weishu.kernelsu.ui.component.material.SearchAppBar
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedItemContainer
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.TonalCard
import me.weishu.kernelsu.ui.component.material.TopBarBackButton
import me.weishu.kernelsu.ui.component.material.expressiveTopAppBarColors
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.screen.module.ExpandableDescriptionText
import me.weishu.kernelsu.ui.theme.LocalModuleDescriptionMaxLines
import me.weishu.kernelsu.ui.util.isDownloadAvailable
import me.weishu.kernelsu.ui.util.rememberContentReady
import top.yukonga.miuix.kmp.utils.PagerNavigationSpringSpec
import java.text.NumberFormat

@SuppressLint("LocalContextGetResourceValueCall")
@Composable
fun ModuleRepoScreenMaterial(
    state: ModuleRepoUiState,
    actions: ModuleRepoActions,
) {
    val haptic = LocalHapticFeedback.current
    val listState = rememberLazyListState()
    val searchListState = rememberLazyListState()
    val refreshTick = remember { mutableIntStateOf(0) }
    val pullToRefreshState = rememberPullToRefreshState()

    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val snackbarHostState = remember { SnackbarHostState() }

    ExpressiveScaffold(
        topBar = {
            SearchAppBar(
                snackbarHostState = snackbarHostState,
                title = { Text(text = stringResource(R.string.module_repos)) },
                searchText = state.searchStatus.searchText,
                onSearchTextChange = actions.onSearchTextChange,
                onClearClick = actions.onClearSearch,
                scrollBehavior = scrollBehavior,
                navigationIcon = {
                    TopBarBackButton(onClick = actions.onBack)
                },
                actions = {
                    var showSortMenu by remember { mutableStateOf(false) }

                    IconButton(
                        onClick = { showSortMenu = true }
                    ) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.Sort,
                            contentDescription = stringResource(R.string.menu_sort)
                        )

                        DropdownMenuPopup(expanded = showSortMenu, onDismissRequest = {
                            showSortMenu = false
                        }) {
                            val sortOptions = listOf(
                                RepoSort.UPDATED to R.string.module_repos_sort_updated,
                                RepoSort.NAME to R.string.module_repos_sort_name,
                                RepoSort.STARS to R.string.module_repos_sort_stars,
                            )
                            DropdownMenuGroup(shapes = MenuDefaults.groupShapes()) {
                                sortOptions.forEachIndexed { index, (order, resId) ->
                                    SelectableDropdownMenuItem(
                                        text = { Text(stringResource(resId)) },
                                        selected = state.sortOrder == order,
                                        onClick = {
                                            haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                            actions.onSetSortOrder(order)
                                            showSortMenu = false
                                        },
                                        shapes = MenuDefaults.itemShape(
                                            index = index,
                                            count = sortOptions.size
                                        ),
                                        selectedLeadingIcon = {
                                            Icon(
                                                Icons.Filled.Check,
                                                contentDescription = null,
                                                modifier = Modifier.size(MenuDefaults.LeadingIconSize),
                                            )
                                        },
                                    )
                                }
                            }
                        }
                    }
                },
                searchContent = { _, closeSearch ->
                    val latestSearchResults = rememberUpdatedState(state.searchResults)
                    ScrollToTopOnChange(
                        searchListState,
                        state.searchStatus.searchText,
                    ) { latestSearchResults.value }
                    RepoModuleList(
                        modules = state.searchResults,
                        installedVersions = state.installedVersions,
                        listState = searchListState,
                        modifier = Modifier.fillMaxSize(),
                        onModuleClick = {
                            closeSearch()
                            actions.onOpenRepoDetail(it)
                        }
                    )
                }
            )
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val isLoading = state.modules.isEmpty()
        val hadDataOnEntry = remember { state.modules.isNotEmpty() }
        val contentReady = hadDataOnEntry || rememberContentReady()

        if (!contentReady || isLoading) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding),
                contentAlignment = Alignment.Center
            ) {
                val emptyMessage = state.emptyStateMessage
                if (emptyMessage != null) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(text = stringResource(emptyMessage), color = MaterialTheme.colorScheme.onSurfaceVariant)
                        Spacer(Modifier.height(12.dp))
                        Button(
                            onClick = actions.onRefresh,
                        ) {
                            Text(stringResource(R.string.network_retry))
                        }
                    }
                } else {
                    LoadingIndicator()
                }
            }
        }
        if (!isLoading && contentReady) {
            val latestModules = rememberUpdatedState(state.modules)
            val latestRefreshing = rememberUpdatedState(state.isRefreshing)
            ScrollToTopOnChange(
                listState,
                state.sortOrder,
                refreshTick.intValue,
                isBusy = { latestRefreshing.value },
            ) { latestModules.value }
            PullToRefreshBox(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding),
                isRefreshing = state.isRefreshing,
                onRefresh = {
                    haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                    actions.onRefresh()
                    refreshTick.intValue++
                },
                state = pullToRefreshState,
                indicator = {
                    PullToRefreshDefaults.LoadingIndicator(
                        modifier = Modifier.align(Alignment.TopCenter),
                        isRefreshing = state.isRefreshing,
                        state = pullToRefreshState,
                    )
                },
            ) {
                RepoModuleList(
                    modules = state.modules,
                    installedVersions = state.installedVersions,
                    stale = state.stale,
                    listState = listState,
                    modifier = Modifier
                        .fillMaxSize()
                        .nestedScroll(scrollBehavior.nestedScrollConnection),
                    onModuleClick = actions.onOpenRepoDetail
                )
            }
        }
    }
}


@Composable
private fun RepoBadges(
    metamodule: Boolean,
    installState: RepoInstallState,
) {
    Row(
        modifier = Modifier.padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        if (metamodule) {
            StatusTag(
                "META",
                modifier = Modifier.padding(bottom = 4.dp),
                contentColor = MaterialTheme.colorScheme.onPrimary,
                backgroundColor = MaterialTheme.colorScheme.primary
            )
        }
        when (installState) {
            RepoInstallState.UPDATABLE -> StatusTag(
                stringResource(R.string.module_repo_update_available),
                modifier = Modifier.padding(bottom = 4.dp),
                contentColor = MaterialTheme.colorScheme.onTertiary,
                backgroundColor = MaterialTheme.colorScheme.tertiary
            )

            RepoInstallState.INSTALLED -> StatusTag(
                stringResource(R.string.module_repo_installed),
                modifier = Modifier.padding(bottom = 4.dp),
                contentColor = MaterialTheme.colorScheme.onSecondaryContainer,
                backgroundColor = MaterialTheme.colorScheme.secondaryContainer
            )

            RepoInstallState.NONE -> {}
        }
    }
}

@Composable
private fun RepoModuleList(
    modules: List<RepoModule>,
    installedVersions: Map<String, Int>,
    listState: LazyListState,
    modifier: Modifier = Modifier,
    stale: Boolean = false,
    bottomPadding: Dp = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding(),
    onModuleClick: (RepoModule) -> Unit,
) {
    LazyColumn(
        modifier = modifier,
        state = listState,
        verticalArrangement = Arrangement.spacedBy(13.dp),
        contentPadding = PaddingValues(
            start = 16.dp,
            end = 16.dp,
            bottom = 16.dp + bottomPadding
        ),
    ) {
        if (stale) {
            item(key = "__notice_stale", contentType = "notice") {
                Text(
                    text = stringResource(R.string.module_repo_using_cache),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 4.dp),
                )
            }
        }
        items(modules, key = { it.moduleId }, contentType = { "module" }) { module ->
            val moduleAuthor = stringResource(id = R.string.module_author)
            val installState = repoInstallState(installedVersions[module.moduleId], module.latestVersionCode)
            val maxLinesLimit = LocalModuleDescriptionMaxLines.current
            var summaryExpanded by rememberSaveable(module.moduleId) { mutableStateOf(false) }

            TonalCard(
                modifier = Modifier.fillMaxWidth(),
                onClick = { onModuleClick(module) }
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp, 14.dp, 16.dp, 10.dp)
                ) {
                    if (module.moduleName.isNotEmpty()) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            RepoModuleIcon(module.iconUrl, Modifier.padding(end = 8.dp))
                            Text(
                                text = module.moduleName,
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                                lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
                            )
                        }
                    }
                    Text(
                        text = "ID: ${module.moduleId}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (module.authors.isNotEmpty()) {
                        Text(
                            text = "$moduleAuthor: ${module.authors}",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    if (module.summary.isNotEmpty()) {
                        ExpandableDescriptionText(
                            text = module.summary,
                            expanded = summaryExpanded,
                            maxLinesLimit = maxLinesLimit,
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(top = 8.dp)
                                .clickable(
                                    interactionSource = remember { MutableInteractionSource() },
                                    indication = null,
                                ) { summaryExpanded = !summaryExpanded },
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    }

                    RepoBadges(module.metamodule, installState)
                    HorizontalDivider(thickness = Dp.Hairline)
                    Spacer(modifier = Modifier.height(4.dp))

                    Row(
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = module.latestVersion,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                        module.stars?.takeIf { it > 0 }?.let { stars ->
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                modifier = Modifier.padding(start = 12.dp),
                            ) {
                                Icon(
                                    imageVector = Icons.Rounded.Star,
                                    contentDescription = stringResource(R.string.module_repos_sort_stars),
                                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.size(16.dp)
                                )
                                Text(
                                    text = formatStarCount(stars),
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.padding(start = 4.dp)
                                )
                            }
                        }
                        Spacer(Modifier.weight(1f))
                        if (module.latestReleaseTime.isNotEmpty()) {
                            Text(
                                text = module.latestReleaseTime,
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
            }
        }
        item { Spacer(Modifier.height(12.dp)) }
    }
}

@SuppressLint("StringFormatInvalid", "DefaultLocale")
@Composable
fun ModuleRepoDetailScreenMaterial(
    state: ModuleRepoDetailUiState,
    actions: ModuleRepoDetailActions,
) {
    val module = state.module
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val confirmTitle = stringResource(R.string.module_install)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })

    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())

    ExpressiveScaffold(
        topBar = {
            LargeFlexibleTopAppBar(
                title = { Text(text = state.detail?.module?.moduleName ?: module.moduleName) },
                navigationIcon = {
                    TopBarBackButton(onClick = actions.onBack)
                },
                actions = {
                    if (state.webUrl.isNotEmpty()) {
                        IconButton(onClick = actions.onOpenWebUrl) {
                            Icon(
                                imageVector = Icons.AutoMirrored.Outlined.ChromeReaderMode,
                                contentDescription = null,
                            )
                        }
                    }
                },
                colors = expressiveTopAppBarColors(),
                scrollBehavior = scrollBehavior
            )
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal),
    ) { innerPadding ->
        val tabs = listOf(
            stringResource(R.string.tab_readme),
            stringResource(R.string.tab_releases),
            stringResource(R.string.tab_info)
        )
        val pagerState = rememberPagerState(initialPage = 0, pageCount = { tabs.size })
        val layoutDirection = LocalLayoutDirection.current
        Box(modifier = Modifier.fillMaxSize()) {
            HorizontalPager(
                state = pagerState,
                modifier = Modifier.fillMaxSize(),
                overscrollEffect = null,
            ) { page ->
                val paddedInnerPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding() + 56.dp + 8.dp,
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection),
                    bottom = innerPadding.calculateBottomPadding()
                            + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding() + 16.dp
                )
                when (page) {
                    0 -> ReadmePage(
                        state = state,
                        onRetry = actions.onRetry,
                        innerPadding = paddedInnerPadding,
                        scrollBehavior = scrollBehavior
                    )

                    1 -> ReleasesPage(
                        state = state,
                        onRetry = actions.onRetry,
                        innerPadding = paddedInnerPadding,
                        scrollBehavior = scrollBehavior,
                        confirmTitle = confirmTitle,
                        confirmDialog = confirmDialog,
                        scope = scope,
                        onInstallModule = actions.onInstallModule,
                        context = context,
                        setPendingDownload = { pendingDownload = it },
                        uriHandler = object : UriHandler {
                            override fun openUri(uri: String) = actions.onOpenUrl(uri)
                        },
                    )

                    2 -> InfoPage(
                        state = state,
                        innerPadding = paddedInnerPadding,
                        scrollBehavior = scrollBehavior,
                        uriHandler = object : UriHandler {
                            override fun openUri(uri: String) = actions.onOpenUrl(uri)
                        },
                    )
                }
            }
            ExpressiveTabRow(
                selectedTabIndex = pagerState.currentPage,
                tabs = tabs,
                onTabClick = {
                    scope.launch {
                        pagerState.animateScrollToPage(
                            page = it,
                            animationSpec = PagerNavigationSpringSpec,
                        )
                    }
                },
                modifier = Modifier.padding(top = innerPadding.calculateTopPadding()),
            )
        }
    }
}

/** Loading / not-in-index / failure placeholder; returns true when the page content can be shown. */
@Composable
private fun LazyItemScope.detailStatus(state: ModuleRepoDetailUiState, onRetry: () -> Unit): Boolean {
    when {
        state.notInIndex -> {
            Box(
                modifier = Modifier
                    .fillParentMaxSize()
                    .padding(horizontal = 24.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = stringResource(R.string.module_repo_not_in_index),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        state.detail == null && state.loaded && state.loadFailed -> {
            Box(modifier = Modifier.fillParentMaxSize(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        text = stringResource(R.string.module_repo_load_failed),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(Modifier.height(12.dp))
                    Button(onClick = onRetry) { Text(stringResource(R.string.network_retry)) }
                }
            }
        }

        state.detail == null -> {
            Box(modifier = Modifier.fillParentMaxSize(), contentAlignment = Alignment.Center) {
                LoadingIndicator()
            }
        }

        else -> return true
    }
    return false
}

/** The detail is the last good cached copy because it could not be refreshed. */
@Composable
private fun CachedDetailNotice(modifier: Modifier = Modifier) {
    Text(
        text = stringResource(R.string.module_repo_using_cache),
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = modifier,
    )
}

@Composable
private fun ReadmePage(
    state: ModuleRepoDetailUiState,
    onRetry: () -> Unit,
    innerPadding: PaddingValues,
    scrollBehavior: TopAppBarScrollBehavior
) {
    val layoutDirection = LocalLayoutDirection.current
    val isReady = rememberContentReady()
    val detail = state.detail

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .nestedScroll(scrollBehavior.nestedScrollConnection),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
            start = innerPadding.calculateStartPadding(layoutDirection),
            end = innerPadding.calculateEndPadding(layoutDirection),
            bottom = innerPadding.calculateBottomPadding(),
        ),
    ) {
        if (detail?.fromCache == true) {
            item(key = "__notice_cached", contentType = "notice") {
                CachedDetailNotice(Modifier.padding(horizontal = 20.dp))
            }
        }
        item {
            if (!detailStatus(state, onRetry) || detail == null) return@item
            val readmeHtml = detail.readmeHtml
            if (readmeHtml != null) {
                var loaded by remember(readmeHtml) { mutableStateOf(false) }
                val alpha by animateFloatAsState(
                    targetValue = if (loaded) 1f else 0f,
                    animationSpec = tween(durationMillis = 300),
                    label = "ReadmeAlpha",
                )
                Box {
                    if (isReady) {
                        Box(modifier = Modifier.graphicsLayer { this.alpha = alpha }) {
                            GithubMarkdown(
                                content = readmeHtml,
                                onLoadingChange = { loaded = !it },
                                containerColor = MaterialTheme.colorScheme.surface,
                                restrictedBaseUrl = detail.readmeBaseUrl,
                            )
                        }
                    }
                    AnimatedVisibility(
                        visible = !loaded,
                        enter = EnterTransition.None,
                        exit = fadeOut(animationSpec = tween(durationMillis = 150)),
                    ) {
                        Box(
                            modifier = Modifier.fillParentMaxSize(),
                            contentAlignment = Alignment.Center,
                        ) {
                            LoadingIndicator()
                        }
                    }
                }
                return@item
            }
            Box(
                modifier = Modifier.fillParentMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                if (detail.readmeFailed) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            text = stringResource(R.string.module_repo_readme_failed),
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Spacer(Modifier.height(12.dp))
                        Button(onClick = onRetry) { Text(stringResource(R.string.network_retry)) }
                    }
                } else {
                    Text(
                        text = stringResource(R.string.module_repo_no_readme),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

@Composable
fun ReleasesPage(
    state: ModuleRepoDetailUiState,
    onRetry: () -> Unit,
    innerPadding: PaddingValues,
    scrollBehavior: TopAppBarScrollBehavior,
    confirmTitle: String,
    confirmDialog: ConfirmDialogHandle,
    scope: CoroutineScope,
    onInstallModule: (Uri) -> Unit,
    context: Context,
    setPendingDownload: ((() -> Unit)) -> Unit,
    uriHandler: UriHandler,
) {
    val layoutDirection = LocalLayoutDirection.current
    val detail = state.detail
    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .nestedScroll(scrollBehavior.nestedScrollConnection),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
            start = innerPadding.calculateStartPadding(layoutDirection) + 16.dp,
            end = innerPadding.calculateEndPadding(layoutDirection) + 16.dp,
            bottom = innerPadding.calculateBottomPadding(),
        ),
        verticalArrangement = Arrangement.spacedBy(13.dp),
    ) {
        if (detail == null) {
            item { detailStatus(state, onRetry) }
            return@LazyColumn
        }
        if (detail.fromCache) {
            item(key = "__notice_cached", contentType = "notice") {
                CachedDetailNotice(Modifier.padding(horizontal = 4.dp))
            }
        }
        items(
            items = detail.releases,
            key = { "release_${it.tag}" },
            contentType = { "release" }
        ) { rel ->
            SegmentedColumn(
                modifier = Modifier.fillMaxWidth(),
                content = buildList<@Composable () -> Unit> {
                    add {
                        SegmentedListItem(
                            headlineContent = { Text(text = rel.name) },
                            supportingContent = {
                                Text(text = if (rel.name == rel.tag) rel.version else "${rel.tag} · ${rel.version}")
                            },
                            trailingContent = rel.publishedAt
                                .takeIf { it.isNotBlank() }
                                ?.let { date ->
                                    { Text(text = date, style = MaterialTheme.typography.bodyMedium) }
                                },
                        )
                    }
                    rel.changelogHtml?.let { changelog ->
                        add {
                            SegmentedItemContainer {
                                val descReady = rememberContentReady()
                                var descLoaded by remember(changelog) { mutableStateOf(false) }
                                val descAlpha by animateFloatAsState(
                                    targetValue = if (descLoaded) 1f else 0f,
                                    animationSpec = tween(durationMillis = 300),
                                    label = "ReleaseDescAlpha",
                                )
                                val descPlaceholderAlpha by animateFloatAsState(
                                    targetValue = if (descLoaded) 0f else 1f,
                                    animationSpec = tween(durationMillis = 150),
                                    label = "ReleaseDescPlaceholderAlpha",
                                )
                                Box(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(horizontal = 16.dp, vertical = 12.dp)
                                        .animateContentSize(animationSpec = tween(durationMillis = 300)),
                                ) {
                                    if (descReady) {
                                        Box(modifier = Modifier.graphicsLayer { this.alpha = descAlpha }) {
                                            GithubMarkdown(
                                                content = changelog,
                                                onLoadingChange = { descLoaded = !it },
                                                restrictedBaseUrl = detail.readmeBaseUrl,
                                            )
                                        }
                                    }
                                    if (descPlaceholderAlpha > 0f) {
                                        Box(
                                            modifier = Modifier
                                                .fillMaxWidth()
                                                .height(72.dp)
                                                .graphicsLayer { this.alpha = descPlaceholderAlpha },
                                            contentAlignment = Alignment.Center,
                                        ) {
                                            LoadingIndicator()
                                        }
                                    }
                                }
                            }
                        }
                    }
                    add {
                        ReleaseZipSegmentedItem(
                            release = rel,
                            canInstall = state.canInstall,
                            confirmTitle = confirmTitle,
                            confirmDialog = confirmDialog,
                            scope = scope,
                            context = context,
                            onInstallModule = onInstallModule,
                            setPendingDownload = setPendingDownload,
                        )
                    }
                    add {
                        InfoLinkItem(
                            headline = stringResource(R.string.module_repo_release_page),
                            supporting = rel.tagCommit.take(12),
                            url = rel.releaseUrl,
                            uriHandler = uriHandler,
                        )
                    }
                    rel.attestationUrl?.let { url ->
                        add { InfoLinkItem(stringResource(R.string.module_repo_attestation), null, url, uriHandler) }
                    }
                }
            )
        }
    }
}

@Composable
private fun ReleaseZipSegmentedItem(
    release: RepoRelease,
    canInstall: Boolean,
    confirmTitle: String,
    confirmDialog: ConfirmDialogHandle,
    scope: CoroutineScope,
    context: Context,
    onInstallModule: (Uri) -> Unit,
    setPendingDownload: ((() -> Unit)) -> Unit,
) {
    val handle = rememberReleaseDownload(release)
    val onClickDownload = {
        val startText = context.getString(R.string.module_start_downloading, release.zipName)
        setPendingDownload { scope.startReleaseDownload(context, release, handle) }
        confirmDialog.showConfirm(title = confirmTitle, content = startText)
    }

    SegmentedListItem(
        headlineContent = { Text(text = release.zipName) },
        supportingContent = {
            Column {
                Text(text = formatFileSize(release.zipSize))
                Text(
                    text = "SHA-256: ${shortHash(release.sha256)}",
                    fontFamily = FontFamily.Monospace,
                    style = MaterialTheme.typography.bodySmall,
                )
                if (release.immutable) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Outlined.Verified,
                            contentDescription = null,
                            modifier = Modifier.size(14.dp),
                        )
                        Text(
                            text = stringResource(R.string.module_repo_immutable_release),
                            style = MaterialTheme.typography.bodySmall,
                            modifier = Modifier.padding(start = 4.dp),
                        )
                    }
                }
            }
        },
        trailingContent = if (!canInstall) null else {
            {
                val downloadedUri = handle.downloadedUri
                if (downloadedUri != null) {
                    FilledTonalButton(
                        onClick = {
                            scope.launch {
                                if (isDownloadAvailable(downloadedUri)) {
                                    onInstallModule(downloadedUri)
                                } else {
                                    handle.downloadedUri = null
                                }
                            }
                        },
                        contentPadding = ButtonDefaults.TextButtonContentPadding
                    ) {
                        Icon(
                            modifier = Modifier.size(20.dp),
                            imageVector = Icons.Outlined.InstallMobile,
                            contentDescription = stringResource(R.string.install)
                        )
                        Text(
                            modifier = Modifier.padding(start = 7.dp),
                            text = stringResource(R.string.install),
                            style = MaterialTheme.typography.labelMedium,
                        )
                    }
                } else {
                    FilledTonalButton(
                        onClick = onClickDownload,
                        enabled = !handle.isDownloading,
                        contentPadding = ButtonDefaults.TextButtonContentPadding
                    ) {
                        if (handle.isDownloading) {
                            CircularWavyProgressIndicator(
                                progress = { handle.progress / 100f },
                                modifier = Modifier.size(20.dp),
                            )
                        } else {
                            Icon(
                                modifier = Modifier.size(20.dp),
                                imageVector = Icons.Outlined.Download,
                                contentDescription = stringResource(R.string.download)
                            )
                            Text(
                                modifier = Modifier.padding(start = 7.dp),
                                text = stringResource(R.string.download),
                                style = MaterialTheme.typography.labelMedium,
                            )
                        }
                    }
                }
            }
        },
    )
}

@Composable
private fun InfoLinkItem(
    headline: String,
    supporting: String?,
    url: String?,
    uriHandler: UriHandler,
) {
    SegmentedListItem(
        headlineContent = {
            Text(
                text = headline,
                fontSize = MaterialTheme.typography.bodyMedium.fontSize,
                lineHeight = MaterialTheme.typography.bodyMedium.lineHeight,
                fontFamily = MaterialTheme.typography.bodyMedium.fontFamily
            )
        },
        supportingContent = supporting?.let { { Text(text = it) } },
        trailingContent = url?.takeIf { it.isNotEmpty() }?.let { link ->
            {
                FilledTonalButton(
                    modifier = Modifier.defaultMinSize(52.dp, 32.dp),
                    onClick = { uriHandler.openUri(link) },
                    contentPadding = ButtonDefaults.TextButtonContentPadding
                ) {
                    Icon(
                        modifier = Modifier.size(20.dp),
                        imageVector = Icons.Outlined.Link,
                        contentDescription = null
                    )
                }
            }
        }
    )
}

@Composable
fun InfoPage(
    state: ModuleRepoDetailUiState,
    innerPadding: PaddingValues,
    scrollBehavior: TopAppBarScrollBehavior,
    uriHandler: UriHandler,
) {
    val layoutDirection = LocalLayoutDirection.current
    val detail = state.detail
    val authors = detail?.module?.authorList?.map { AuthorArg(it.name, it.link) } ?: state.module.authorsList
    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .nestedScroll(scrollBehavior.nestedScrollConnection),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
            start = innerPadding.calculateStartPadding(layoutDirection),
            end = innerPadding.calculateEndPadding(layoutDirection),
            bottom = innerPadding.calculateBottomPadding(),
        ),
    ) {
        item {
            Column(modifier = Modifier.padding(horizontal = 16.dp)) {
                if (state.notInIndex) {
                    Text(
                        text = stringResource(R.string.module_repo_not_in_index),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.error,
                    )
                }
                if (detail?.healthStale == true) {
                    Text(
                        text = stringResource(R.string.module_repo_health_stale),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (detail?.fromCache == true) {
                    Text(
                        text = stringResource(R.string.module_repo_using_cache),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
        if (authors.isNotEmpty()) {
            item {
                SegmentedColumn(
                    title = stringResource(R.string.module_author),
                    modifier = Modifier
                        .padding(horizontal = 16.dp)
                        .padding(bottom = 8.dp),
                    content = authors.map { author ->
                        { InfoLinkItem(author.name, null, author.link, uriHandler) }
                    }
                )
            }
        }
        if (detail != null) {
            item {
                SegmentedColumn(
                    title = stringResource(R.string.module_repos_source_code),
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    content = listOf {
                        InfoLinkItem(
                            headline = detail.sourceFullName,
                            supporting = stringResource(R.string.module_repo_source_commit, detail.sourceCommit.take(12)),
                            url = detail.sourceUrl,
                            uriHandler = uriHandler,
                        )
                    }
                )
            }
            item {
                val versionText = "${detail.module.latestVersion} (${detail.module.latestVersionCode})"
                SegmentedColumn(
                    title = stringResource(R.string.tab_info),
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    content = buildList<@Composable () -> Unit> {
                        add { InfoLinkItem(stringResource(R.string.module_version), versionText, null, uriHandler) }
                        detail.module.tags.takeIf { it.isNotEmpty() }?.let { tags ->
                            add { InfoLinkItem(stringResource(R.string.module_repo_tags), tags.joinToString(", "), null, uriHandler) }
                        }
                        detail.module.stars?.let { stars ->
                            add {
                                InfoLinkItem(
                                    stringResource(R.string.module_repo_stars),
                                    listOfNotNull(
                                        NumberFormat.getIntegerInstance().format(stars),
                                        detail.module.starsRepository,
                                    ).joinToString(" · "),
                                    starsUrl(detail.module.starsRepository),
                                    uriHandler
                                )
                            }
                        }
                        detail.license?.let { license ->
                            add { InfoLinkItem(stringResource(R.string.module_repo_license), license, null, uriHandler) }
                        }
                        detail.homepage?.let { url ->
                            add { InfoLinkItem(stringResource(R.string.module_repo_homepage), url, url, uriHandler) }
                        }
                        detail.support?.let { url ->
                            add { InfoLinkItem(stringResource(R.string.module_repo_support), url, url, uriHandler) }
                        }
                        add {
                            InfoLinkItem(
                                stringResource(R.string.module_repo_integrity),
                                stringResource(R.string.module_repo_integrity_desc),
                                null,
                                uriHandler
                            )
                        }
                    }
                )
            }
        }
    }
}
