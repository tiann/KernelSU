package me.weishu.kernelsu.ui.screen.modulerepo

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
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
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyItemScope
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.RepoRelease
import me.weishu.kernelsu.ui.component.ListPopupDefaults
import top.yukonga.miuix.kmp.utils.PagerNavigationSpringSpec
import me.weishu.kernelsu.ui.component.ScrollToTopOnChange
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.component.dialog.ConfirmDialogHandle
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.markdown.GithubMarkdown
import me.weishu.kernelsu.ui.component.miuix.SearchBarFake
import me.weishu.kernelsu.ui.component.miuix.SearchBox
import me.weishu.kernelsu.ui.component.miuix.SearchPager
import me.weishu.kernelsu.ui.component.miuix.deferredTopPadding
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.isDownloadAvailable
import me.weishu.kernelsu.ui.util.rememberBlurBackdrop
import me.weishu.kernelsu.ui.util.rememberContentReady
import java.text.NumberFormat
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CircularProgressIndicator
import top.yukonga.miuix.kmp.basic.DropdownImpl
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.RefreshState
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.SmallTitle
import top.yukonga.miuix.kmp.basic.TabRow
import top.yukonga.miuix.kmp.basic.TabRowDefaults
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import top.yukonga.miuix.kmp.blur.LayerBackdrop
import top.yukonga.miuix.kmp.blur.layerBackdrop
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.icon.extended.FileDownloads
import top.yukonga.miuix.kmp.icon.extended.HorizontalSplit
import top.yukonga.miuix.kmp.icon.extended.Link
import top.yukonga.miuix.kmp.icon.extended.Sort
import top.yukonga.miuix.kmp.icon.extended.TopDownloads
import top.yukonga.miuix.kmp.overlay.OverlayListPopup
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@SuppressLint("LocalContextGetResourceValueCall")
@Composable
fun ModuleRepoScreenMiuix(
    state: ModuleRepoUiState,
    actions: ModuleRepoActions,
) {
    val searchStatus = state.searchStatus
    val density = LocalDensity.current

    LaunchedEffect(searchStatus.searchText) {
        actions.onSearchTextChange(searchStatus.searchText)
    }

    val scrollBehavior = MiuixScrollBehavior()
    val dynamicTopPadding by remember {
        derivedStateOf { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }

    val enableBlur = LocalEnableBlur.current
    val backdrop = rememberBlurBackdrop(enableBlur)
    val blurActive = backdrop != null
    val barColor = if (blurActive) Color.Transparent else colorScheme.surface

    Scaffold(
        topBar = {
            BlurredBar(backdrop) {
                searchStatus.TopAppBarAnim(backgroundColor = barColor) {
                    TopAppBar(
                        color = barColor,
                        title = stringResource(R.string.module_repos),
                        actions = {
                            val showSortPopup = remember { mutableStateOf(false) }
                            OverlayListPopup(
                                show = showSortPopup.value,
                                popupPositionProvider = ListPopupDefaults.MenuPositionProvider,
                                alignment = PopupPositionProvider.Align.TopEnd,
                                onDismissRequest = { showSortPopup.value = false },
                                content = {
                                    ListPopupColumn {
                                        val sortOptions = listOf(
                                            RepoSort.UPDATED to R.string.module_repos_sort_updated,
                                            RepoSort.NAME to R.string.module_repos_sort_name,
                                            RepoSort.STARS to R.string.module_repos_sort_stars,
                                        )
                                        sortOptions.forEachIndexed { index, (order, resId) ->
                                            DropdownImpl(
                                                text = stringResource(resId),
                                                optionSize = sortOptions.size,
                                                isSelected = state.sortOrder == order,
                                                onSelectedIndexChange = {
                                                    actions.onSetSortOrder(order)
                                                    showSortPopup.value = false
                                                },
                                                index = index,
                                            )
                                        }
                                    }
                                })
                            IconButton(
                                onClick = { showSortPopup.value = true },
                                holdDownState = showSortPopup.value
                            ) {
                                Icon(
                                    imageVector = MiuixIcons.Sort,
                                    tint = colorScheme.onSurface,
                                    contentDescription = stringResource(R.string.menu_sort),
                                )
                            }
                        },
                        navigationIcon = {
                            IconButton(
                                onClick = actions.onBack
                            ) {
                                val layoutDirection = LocalLayoutDirection.current
                                Icon(
                                    modifier = Modifier.graphicsLayer {
                                        if (layoutDirection == LayoutDirection.Rtl) scaleX = -1f
                                    },
                                    imageVector = MiuixIcons.Back,
                                    contentDescription = null,
                                    tint = colorScheme.onSurface
                                )
                            }
                        },
                        scrollBehavior = scrollBehavior, bottomContent = {
                            Box(
                                modifier = Modifier
                                    .alpha(if (searchStatus.isCollapsed()) 1f else 0f)
                                    .onGloballyPositioned { coordinates ->
                                        with(density) {
                                            val newOffsetY = coordinates.positionInWindow().y.toDp()
                                            if (searchStatus.offsetY != newOffsetY) {
                                                actions.onSearchStatusChange(searchStatus.copy(offsetY = newOffsetY))
                                            }
                                        }
                                    }
                                    .then(
                                        if (searchStatus.isCollapsed()) {
                                            Modifier.pointerInput(Unit) {
                                                detectTapGestures {
                                                    actions.onSearchStatusChange(searchStatus.copy(current = SearchStatus.Status.EXPANDING))
                                                }
                                            }
                                        } else Modifier,
                                    ),
                            ) {
                                SearchBarFake(searchStatus.label, dynamicTopPadding)
                            }
                        }
                    )
                }
            }
        },
        popupHost = {
            searchStatus.SearchPager(
                onSearchStatusChange = actions.onSearchStatusChange,
                defaultResult = {},
                searchBarTopPadding = dynamicTopPadding,
            ) {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .overScrollVertical(),
                ) {
                    item {
                        Spacer(Modifier.height(6.dp))
                    }
                    items(state.searchResults, key = { it.moduleId }, contentType = { "module" }) { module ->
                        Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 12.dp)
                                .padding(bottom = 12.dp),
                            insideMargin = PaddingValues(16.dp),
                            showIndication = true,
                            pressFeedbackType = PressFeedbackType.Sink,
                            onClick = {
                                actions.onOpenRepoDetail(module)
                            }
                        ) {
                            Column {
                                if (module.moduleName.isNotEmpty()) {
                                    Row(verticalAlignment = Alignment.CenterVertically) {
                                        RepoModuleIcon(module.iconUrl, Modifier.padding(end = 8.dp))
                                        Row(
                                            modifier = Modifier.weight(1f),
                                            verticalAlignment = Alignment.CenterVertically,
                                        ) {
                                            Text(
                                                text = module.moduleName,
                                                fontSize = 17.sp,
                                                fontWeight = FontWeight(550),
                                                color = colorScheme.onSurface,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis,
                                                modifier = Modifier.weight(1f, fill = false),
                                            )
                                            RepoBadgesMiuix(
                                                metamodule = module.metamodule,
                                                installState = repoInstallState(
                                                    state.installedVersions[module.moduleId],
                                                    module.latestVersionCode
                                                ),
                                            )
                                        }
                                        RepoStarsMiuix(module.stars, Modifier.padding(start = 8.dp))
                                    }
                                }
                                if (module.moduleId.isNotEmpty()) {
                                    Text(
                                        text = "ID: ${module.moduleId}",
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight(550),
                                        color = colorScheme.onSurfaceVariantSummary,
                                    )
                                }
                                Text(
                                    text = "${stringResource(id = R.string.module_author)}: ${module.authors}",
                                    fontSize = 12.sp,
                                    modifier = Modifier.padding(bottom = 1.dp),
                                    fontWeight = FontWeight(550),
                                    color = colorScheme.onSurfaceVariantSummary,
                                )
                                if (module.summary.isNotEmpty()) {
                                    Text(
                                        text = module.summary,
                                        fontSize = 14.sp,
                                        color = colorScheme.onSurfaceVariantSummary,
                                        modifier = Modifier.padding(top = 2.dp),
                                        overflow = TextOverflow.Ellipsis,
                                        maxLines = 4,
                                    )
                                }
                            }
                        }
                    }
                }
            }
        },
    ) { innerPadding ->
        val layoutDirection = LocalLayoutDirection.current
        val isLoading = state.modules.isEmpty()
        val hadDataOnEntry = remember { state.modules.isNotEmpty() }
        val contentReady = hadDataOnEntry || rememberContentReady()
        val emptyMessage = state.emptyStateMessage

        searchStatus.SearchBox {
            val pullToRefreshState = rememberPullToRefreshState()
            val lazyListState = rememberLazyListState()
            val refreshTick = remember { mutableIntStateOf(0) }
            val latestModules = rememberUpdatedState(state.modules)
            val latestRefreshing = rememberUpdatedState(state.isRefreshing)
            ScrollToTopOnChange(
                lazyListState,
                state.sortOrder,
                refreshTick.intValue,
                isBusy = { latestRefreshing.value },
            ) { latestModules.value }
            val refreshTexts = listOf(
                stringResource(R.string.refresh_pulling),
                stringResource(R.string.refresh_release),
                stringResource(R.string.refresh_refresh),
                stringResource(R.string.refresh_complete),
            )
            PullToRefresh(
                isRefreshing = state.isRefreshing,
                pullToRefreshState = pullToRefreshState,
                onRefresh = {
                    actions.onRefresh()
                    refreshTick.intValue++
                },
                refreshTexts = refreshTexts,
                contentPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding() + 6.dp,
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection)
                ),
            ) {
                if (!contentReady || isLoading) {
                    Box(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(
                                top = innerPadding.calculateTopPadding(),
                                start = innerPadding.calculateStartPadding(layoutDirection),
                                end = innerPadding.calculateEndPadding(layoutDirection),
                            ),
                        contentAlignment = Alignment.Center
                    ) {
                        if (emptyMessage != null) {
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Text(
                                    text = stringResource(emptyMessage),
                                    color = colorScheme.onSurfaceVariantSummary,
                                    fontSize = 16.sp
                                )
                                Spacer(Modifier.height(12.dp))
                                TextButton(
                                    modifier = Modifier
                                        .padding(horizontal = 24.dp)
                                        .fillMaxWidth(),
                                    text = stringResource(R.string.network_retry),
                                    onClick = actions.onRefresh,
                                )
                            }
                        } else if (pullToRefreshState.refreshState == RefreshState.Idle) {
                            InfiniteProgressIndicator()
                        }
                    }
                } else {
                    Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
                        LazyColumn(
                            state = lazyListState,
                            modifier = Modifier
                                .fillMaxHeight()
                                .scrollEndHaptic()
                                .overScrollVertical()
                                .nestedScroll(scrollBehavior.nestedScrollConnection),
                            contentPadding = PaddingValues(
                                top = innerPadding.calculateTopPadding() + 6.dp,
                                start = innerPadding.calculateStartPadding(layoutDirection),
                                end = innerPadding.calculateEndPadding(layoutDirection)
                            ),
                            overscrollEffect = null,
                        ) {
                            if (state.stale) {
                                item(key = "__notice_stale", contentType = "notice") {
                                    Text(
                                        text = stringResource(R.string.module_repo_using_cache),
                                        fontSize = 12.sp,
                                        color = colorScheme.onSurfaceVariantSummary,
                                        modifier = Modifier.padding(horizontal = 24.dp).padding(bottom = 8.dp),
                                    )
                                }
                            }
                            items(items = state.modules, key = { it.moduleId }, contentType = { "module" }) { module ->
                                val moduleAuthor = stringResource(id = R.string.module_author)

                                Card(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(horizontal = 12.dp)
                                        .padding(bottom = 12.dp),
                                    insideMargin = PaddingValues(16.dp),
                                    showIndication = true,
                                    onClick = { actions.onOpenRepoDetail(module) }) {
                                    Column {
                                        if (module.moduleName.isNotEmpty()) {
                                            Row(verticalAlignment = Alignment.CenterVertically) {
                                                RepoModuleIcon(module.iconUrl, Modifier.padding(end = 8.dp))
                                                Text(
                                                    text = module.moduleName,
                                                    fontSize = 17.sp,
                                                    fontWeight = FontWeight(550),
                                                    color = colorScheme.onSurface,
                                                    maxLines = 1,
                                                    overflow = TextOverflow.Ellipsis,
                                                    modifier = Modifier.weight(1f, fill = false),
                                                )
                                                RepoBadgesMiuix(
                                                    metamodule = module.metamodule,
                                                    installState = repoInstallState(
                                                        state.installedVersions[module.moduleId],
                                                        module.latestVersionCode
                                                    ),
                                                )
                                            }
                                        }
                                        if (module.moduleId.isNotEmpty()) {
                                            Text(
                                                text = "ID: ${module.moduleId}",
                                                fontSize = 12.sp,
                                                fontWeight = FontWeight(550),
                                                color = colorScheme.onSurfaceVariantSummary,
                                            )
                                        }
                                        if (module.authors.isNotEmpty()) Text(
                                            text = "$moduleAuthor: ${module.authors}",
                                            fontSize = 12.sp,
                                            modifier = Modifier.padding(bottom = 1.dp),
                                            fontWeight = FontWeight(550),
                                            color = colorScheme.onSurfaceVariantSummary,
                                        )
                                        if (module.summary.isNotEmpty()) {
                                            Text(
                                                text = module.summary,
                                                fontSize = 14.sp,
                                                color = colorScheme.onSurfaceVariantSummary,
                                                modifier = Modifier.padding(top = 2.dp),
                                                overflow = TextOverflow.Ellipsis,
                                                maxLines = 4,
                                            )
                                        }
                                        HorizontalDivider(
                                            modifier = Modifier.padding(vertical = 8.dp),
                                            thickness = 0.5.dp,
                                            color = colorScheme.outline.copy(alpha = 0.5f)
                                        )
                                        Row(
                                            verticalAlignment = Alignment.CenterVertically,
                                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                                        ) {
                                            Row {
                                                Text(
                                                    text = module.latestVersion,
                                                    fontSize = 12.sp,
                                                    color = colorScheme.onSurfaceVariantSummary,
                                                    maxLines = 1,
                                                    overflow = TextOverflow.Ellipsis,
                                                    modifier = Modifier.weight(1f, fill = false),
                                                )
                                                RepoStarsMiuix(module.stars, Modifier.padding(start = 12.dp))
                                                Spacer(Modifier.weight(1f))
                                                if (module.latestReleaseTime.isNotEmpty()) {
                                                    Text(
                                                        text = module.latestReleaseTime,
                                                        fontSize = 12.sp,
                                                        color = colorScheme.onSurfaceVariantSummary,
                                                        textAlign = TextAlign.End
                                                    )
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            item {
                                Spacer(Modifier.height(WindowInsets.systemBars.asPaddingValues().calculateBottomPadding()))
                            }
                        }
                    }
                }
            }
        }
    }
}

/** Star icon + compact count; nothing when the count is unknown or 0. */
@Composable
private fun RepoStarsMiuix(stars: Int?, modifier: Modifier = Modifier) {
    if (stars == null || stars <= 0) return
    Row(verticalAlignment = Alignment.CenterVertically, modifier = modifier) {
        Icon(
            imageVector = MiuixIcons.TopDownloads,
            contentDescription = stringResource(R.string.module_repos_sort_stars),
            tint = colorScheme.onSurfaceVariantSummary,
            modifier = Modifier.size(16.dp)
        )
        Text(
            text = formatStarCount(stars),
            fontSize = 12.sp,
            color = colorScheme.onSurfaceVariantSummary,
            modifier = Modifier.padding(start = 4.dp)
        )
    }
}

@Composable
private fun MiuixTag(text: String, background: Color, tint: Color) {
    Text(
        text = text,
        fontSize = 12.sp,
        color = tint,
        modifier = Modifier
            .padding(start = 6.dp)
            .clip(RoundedCornerShape(6.dp))
            .background(background)
            .padding(horizontal = 6.dp, vertical = 2.dp),
        fontWeight = FontWeight(750),
        maxLines = 1
    )
}

@Composable
private fun RepoBadgesMiuix(
    metamodule: Boolean,
    installState: RepoInstallState,
) {
    if (metamodule) {
        MiuixTag(
            text = "META",
            background = colorScheme.tertiaryContainer.copy(alpha = 0.6f),
            tint = colorScheme.onTertiaryContainer.copy(alpha = 0.8f),
        )
    }
    when (installState) {
        RepoInstallState.UPDATABLE -> MiuixTag(
            text = stringResource(R.string.module_repo_update_available),
            background = colorScheme.primary.copy(alpha = 0.15f),
            tint = colorScheme.primary,
        )

        RepoInstallState.INSTALLED -> MiuixTag(
            text = stringResource(R.string.module_repo_installed),
            background = colorScheme.secondaryContainer.copy(alpha = 0.8f),
            tint = colorScheme.onSecondaryContainer,
        )

        RepoInstallState.NONE -> {}
    }
}

/** Loading / not-in-index / failure placeholder; returns true when the page content can be shown. */
@Composable
private fun LazyItemScope.DetailStatusMiuix(state: ModuleRepoDetailUiState, onRetry: () -> Unit): Boolean {
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
                    color = colorScheme.onSurfaceVariantSummary,
                    textAlign = TextAlign.Center,
                )
            }
        }

        state.detail == null && state.loaded && state.loadFailed -> {
            Box(modifier = Modifier.fillParentMaxSize(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        text = stringResource(R.string.module_repo_load_failed),
                        color = colorScheme.onSurfaceVariantSummary,
                        fontSize = 16.sp
                    )
                    Spacer(Modifier.height(12.dp))
                    TextButton(
                        modifier = Modifier
                            .padding(horizontal = 24.dp)
                            .fillMaxWidth(),
                        text = stringResource(R.string.network_retry),
                        onClick = onRetry,
                    )
                }
            }
        }

        state.detail == null -> {
            Box(modifier = Modifier.fillParentMaxSize(), contentAlignment = Alignment.Center) {
                InfiniteProgressIndicator()
            }
        }

        else -> return true
    }
    return false
}

/** The detail is the last good cached copy because it could not be refreshed. */
@Composable
private fun CachedDetailNoticeMiuix(modifier: Modifier = Modifier) {
    Text(
        text = stringResource(R.string.module_repo_using_cache),
        fontSize = 12.sp,
        color = colorScheme.onSurfaceVariantSummary,
        modifier = modifier,
    )
}

@Composable
private fun ReadmePage(
    state: ModuleRepoDetailUiState,
    onRetry: () -> Unit,
    innerPadding: PaddingValues, scrollBehavior: ScrollBehavior, backdrop: LayerBackdrop?
) {
    val layoutDirection = LocalLayoutDirection.current
    val isReady = rememberContentReady()
    val detail = state.detail
    Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
        LazyColumn(
            modifier = Modifier
                .fillMaxHeight()
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection),
            contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding(),
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection),
                bottom = innerPadding.calculateBottomPadding(),
            ),
            overscrollEffect = null,
        ) {
            if (detail?.fromCache == true) {
                item(key = "__notice_cached", contentType = "notice") {
                    CachedDetailNoticeMiuix(Modifier.padding(horizontal = 24.dp).padding(top = 6.dp))
                }
            }
            item {
                if (!DetailStatusMiuix(state, onRetry) || detail == null) return@item
                val readmeHtml = detail.readmeHtml
                if (readmeHtml != null) {
                    var loaded by remember(readmeHtml) { mutableStateOf(false) }
                    val alpha by animateFloatAsState(
                        targetValue = if (loaded) 1f else 0f,
                        animationSpec = tween(durationMillis = 300),
                        label = "MiuixReadmeAlpha",
                    )
                    val placeholderAlpha by animateFloatAsState(
                        targetValue = if (loaded) 0f else 1f,
                        animationSpec = tween(durationMillis = 150),
                        label = "MiuixReadmePlaceholderAlpha",
                    )
                    Box {
                        if (isReady) {
                            Box(modifier = Modifier.graphicsLayer { this.alpha = alpha }) {
                                GithubMarkdown(
                                    content = readmeHtml,
                                    onLoadingChange = { loaded = !it },
                                    restrictedBaseUrl = detail.readmeBaseUrl,
                                )
                            }
                        }
                        if (placeholderAlpha > 0f) {
                            Box(
                                modifier = Modifier
                                    .fillParentMaxSize()
                                    .graphicsLayer { this.alpha = placeholderAlpha },
                                contentAlignment = Alignment.Center,
                            ) {
                                InfiniteProgressIndicator()
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
                                color = colorScheme.onSurfaceVariantSummary,
                            )
                            Spacer(Modifier.height(12.dp))
                            TextButton(
                                text = stringResource(R.string.network_retry),
                                onClick = onRetry,
                            )
                        }
                    } else {
                        Text(
                            text = stringResource(R.string.module_repo_no_readme),
                            color = colorScheme.onSurfaceVariantSummary,
                        )
                    }
                }
            }
            item { Spacer(Modifier.height(12.dp)) }
        }
    }
}

@Composable
fun ReleasesPage(
    state: ModuleRepoDetailUiState,
    onRetry: () -> Unit,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    backdrop: LayerBackdrop?,
    actionIconTint: Color,
    secondaryContainer: Color,
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
    Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
        LazyColumn(
            modifier = Modifier
                .fillMaxHeight()
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection),
            contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding(),
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection),
                bottom = innerPadding.calculateBottomPadding(),
            ),
            overscrollEffect = null,
        ) {
            if (detail == null) {
                item { DetailStatusMiuix(state, onRetry) }
                return@LazyColumn
            }
            item {
                Spacer(Modifier.height(6.dp))
            }
            if (detail.fromCache) {
                item(key = "__notice_cached", contentType = "notice") {
                    CachedDetailNoticeMiuix(Modifier.padding(horizontal = 24.dp).padding(bottom = 8.dp))
                }
            }
            items(items = detail.releases, key = { "release_${it.tag}" }, contentType = { "release" }) { rel ->
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 12.dp)
                        .padding(bottom = 12.dp)
                ) {
                    Column {
                        Row(
                            verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()
                        ) {
                            Column(
                                modifier = Modifier
                                    .padding(start = 16.dp, end = 16.dp, top = 16.dp)
                                    .weight(1f)
                            ) {
                                Text(
                                    text = rel.name, fontSize = 17.sp, fontWeight = FontWeight(550), color = colorScheme.onSurface
                                )
                                Text(
                                    text = if (rel.name == rel.tag) rel.version else "${rel.tag} · ${rel.version}",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight(550),
                                    color = colorScheme.onSurfaceVariantSummary,
                                    modifier = Modifier.padding(top = 2.dp)
                                )
                            }
                            Text(
                                text = rel.publishedAt,
                                fontSize = 12.sp,
                                color = colorScheme.onSurfaceVariantSummary,
                                modifier = Modifier
                                    .padding(start = 16.dp, end = 16.dp, top = 16.dp)
                                    .align(Alignment.Top)
                            )
                        }
                        val changelog = rel.changelogHtml
                        if (changelog != null) {
                            Column {
                                HorizontalDivider(
                                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 4.dp),
                                    thickness = 0.5.dp,
                                    color = colorScheme.outline.copy(alpha = 0.5f)
                                )
                                val descReady = rememberContentReady()
                                var descLoaded by remember(changelog) { mutableStateOf(false) }
                                val descAlpha by animateFloatAsState(
                                    targetValue = if (descLoaded) 1f else 0f,
                                    animationSpec = tween(durationMillis = 300),
                                    label = "MiuixReleaseDescAlpha",
                                )
                                val descPlaceholderAlpha by animateFloatAsState(
                                    targetValue = if (descLoaded) 0f else 1f,
                                    animationSpec = tween(durationMillis = 150),
                                    label = "MiuixDescPlaceholderAlpha",
                                )
                                Box(
                                    modifier = Modifier
                                        .fillMaxWidth()
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
                                            InfiniteProgressIndicator()
                                        }
                                    }
                                }
                            }
                        }
                        HorizontalDivider(
                            modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 8.dp),
                            thickness = 0.5.dp,
                            color = colorScheme.outline.copy(alpha = 0.5f)
                        )
                        ReleaseZipRowMiuix(
                            release = rel,
                            canInstall = state.canInstall,
                            actionIconTint = actionIconTint,
                            secondaryContainer = secondaryContainer,
                            confirmTitle = confirmTitle,
                            confirmDialog = confirmDialog,
                            scope = scope,
                            onInstallModule = onInstallModule,
                            context = context,
                            setPendingDownload = setPendingDownload,
                        )
                        InfoRowMiuix(
                            stringResource(R.string.module_repo_release_page), rel.tagCommit.take(12), rel.releaseUrl,
                            actionIconTint, secondaryContainer, uriHandler
                        )
                        rel.attestationUrl?.let { url ->
                            InfoRowMiuix(
                                stringResource(R.string.module_repo_attestation), null, url,
                                actionIconTint, secondaryContainer, uriHandler
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ReleaseZipRowMiuix(
    release: RepoRelease,
    canInstall: Boolean,
    actionIconTint: Color,
    secondaryContainer: Color,
    confirmTitle: String,
    confirmDialog: ConfirmDialogHandle,
    scope: CoroutineScope,
    onInstallModule: (Uri) -> Unit,
    context: Context,
    setPendingDownload: ((() -> Unit)) -> Unit,
) {
    val handle = rememberReleaseDownload(release)
    val onClickDownload = {
        val startText = context.getString(R.string.module_start_downloading, release.zipName)
        setPendingDownload { scope.startReleaseDownload(context, release, handle) }
        confirmDialog.showConfirm(title = confirmTitle, content = startText)
    }
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Column(
            modifier = Modifier
                .padding(start = 16.dp, end = 16.dp, bottom = 16.dp)
                .weight(1f)
        ) {
            Text(text = release.zipName, fontSize = 14.sp, color = colorScheme.onSurface)
            Text(
                text = formatFileSize(release.zipSize),
                fontSize = 12.sp,
                color = colorScheme.onSurfaceVariantSummary,
                modifier = Modifier.padding(top = 2.dp)
            )
            Text(
                text = "SHA-256: ${shortHash(release.sha256)}",
                fontSize = 12.sp,
                fontFamily = FontFamily.Monospace,
                color = colorScheme.onSurfaceVariantSummary,
            )
            if (release.immutable) {
                Text(
                    text = stringResource(R.string.module_repo_immutable_release),
                    fontSize = 12.sp,
                    color = colorScheme.onSurfaceVariantSummary,
                )
            }
        }
        if (!canInstall) return@Row
        val downloadedUri = handle.downloadedUri
        IconButton(
            modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 16.dp),
            backgroundColor = secondaryContainer,
            minHeight = 35.dp,
            minWidth = 35.dp,
            enabled = downloadedUri != null || !handle.isDownloading,
            onClick = {
                if (downloadedUri != null) {
                    scope.launch {
                        if (isDownloadAvailable(downloadedUri)) {
                            onInstallModule(downloadedUri)
                        } else {
                            handle.downloadedUri = null
                        }
                    }
                } else {
                    onClickDownload()
                }
            },
        ) {
            if (downloadedUri == null && handle.isDownloading) {
                CircularProgressIndicator(
                    progress = handle.progress / 100f, size = 20.dp, strokeWidth = 2.dp
                )
            } else {
                val label = stringResource(if (downloadedUri != null) R.string.install else R.string.download)
                Row(
                    modifier = Modifier.padding(horizontal = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(
                        modifier = Modifier.size(20.dp),
                        imageVector = MiuixIcons.FileDownloads,
                        tint = actionIconTint,
                        contentDescription = label
                    )
                    Text(
                        modifier = Modifier.padding(start = 4.dp, end = 2.dp),
                        text = label,
                        color = actionIconTint,
                        fontWeight = FontWeight.Medium,
                        fontSize = 15.sp
                    )
                }
            }
        }
    }
}

@Composable
private fun InfoRowMiuix(
    title: String,
    value: String?,
    url: String?,
    actionIconTint: Color,
    secondaryContainer: Color,
    uriHandler: UriHandler,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, fontSize = 14.sp, color = colorScheme.onSurface)
            if (!value.isNullOrEmpty()) {
                Text(
                    text = value,
                    fontSize = 12.sp,
                    color = colorScheme.onSurfaceVariantSummary,
                    modifier = Modifier.padding(top = 2.dp)
                )
            }
        }
        if (!url.isNullOrEmpty()) {
            IconButton(
                backgroundColor = secondaryContainer,
                minHeight = 35.dp,
                minWidth = 35.dp,
                onClick = { uriHandler.openUri(url) },
            ) {
                Icon(
                    modifier = Modifier.size(20.dp),
                    imageVector = MiuixIcons.Link,
                    tint = actionIconTint,
                    contentDescription = null
                )
            }
        }
    }
}

@Composable
private fun InfoCardMiuix(title: String, rows: List<@Composable () -> Unit>) {
    if (rows.isEmpty()) return
    SmallTitle(text = title, modifier = Modifier.padding(top = 6.dp))
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp)
            .padding(bottom = 6.dp),
        insideMargin = PaddingValues(16.dp)
    ) {
        Column {
            rows.forEachIndexed { index, row ->
                row()
                if (index != rows.lastIndex) {
                    HorizontalDivider(
                        modifier = Modifier.padding(vertical = 8.dp),
                        thickness = 0.5.dp,
                        color = colorScheme.outline.copy(alpha = 0.5f)
                    )
                }
            }
        }
    }
}

@Composable
fun InfoPage(
    state: ModuleRepoDetailUiState,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    backdrop: LayerBackdrop?,
    actionIconTint: Color,
    secondaryContainer: Color,
    uriHandler: UriHandler,
) {
    val layoutDirection = LocalLayoutDirection.current
    val detail = state.detail
    val authors = detail?.module?.authorList?.map { AuthorArg(it.name, it.link) } ?: state.module.authorsList
    val metamodule = detail?.module?.metamodule ?: state.module.metamodule
    val installState = repoInstallState(
        state.installedVersionCode,
        detail?.module?.latestVersionCode ?: state.module.latestVersionCode
    )
    Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
        LazyColumn(
            modifier = Modifier
                .fillMaxHeight()
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection),
            contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding(),
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection),
                bottom = innerPadding.calculateBottomPadding(),
            ),
            overscrollEffect = null,
        ) {
            item {
                Column(modifier = Modifier.padding(horizontal = 18.dp, vertical = 6.dp)) {
                    if (metamodule || installState != RepoInstallState.NONE) {
                        Row(modifier = Modifier.padding(bottom = 4.dp)) {
                            RepoBadgesMiuix(metamodule, installState)
                        }
                    }
                    if (state.notInIndex) {
                        Text(
                            text = stringResource(R.string.module_repo_not_in_index),
                            fontSize = 14.sp,
                            color = colorScheme.error,
                        )
                    }
                    if (detail?.healthStale == true) {
                        Text(
                            text = stringResource(R.string.module_repo_health_stale),
                            fontSize = 14.sp,
                            color = colorScheme.onSurfaceVariantSummary,
                        )
                    }
                    if (detail?.fromCache == true) {
                        Text(
                            text = stringResource(R.string.module_repo_using_cache),
                            fontSize = 14.sp,
                            color = colorScheme.onSurfaceVariantSummary,
                        )
                    }
                }
            }
            if (authors.isNotEmpty()) {
                item {
                    InfoCardMiuix(
                        title = stringResource(R.string.module_author),
                        rows = authors.map { author ->
                            { InfoRowMiuix(author.name, null, author.link, actionIconTint, secondaryContainer, uriHandler) }
                        }
                    )
                }
            }
            if (detail != null) {
                item {
                    InfoCardMiuix(
                        title = stringResource(R.string.module_repos_source_code),
                        rows = listOf {
                            InfoRowMiuix(
                                detail.sourceFullName,
                                stringResource(R.string.module_repo_source_commit, detail.sourceCommit.take(12)),
                                detail.sourceUrl,
                                actionIconTint,
                                secondaryContainer,
                                uriHandler
                            )
                        }
                    )
                }
                item {
                    val versionText = "${detail.module.latestVersion} (${detail.module.latestVersionCode})"
                    InfoCardMiuix(
                        title = stringResource(R.string.tab_info),
                        rows = buildList<@Composable () -> Unit> {
                            add {
                                InfoRowMiuix(
                                    stringResource(R.string.module_version), versionText, null,
                                    actionIconTint, secondaryContainer, uriHandler
                                )
                            }
                            detail.module.tags.takeIf { it.isNotEmpty() }?.let { tags ->
                                add {
                                    InfoRowMiuix(
                                        stringResource(R.string.module_repo_tags), tags.joinToString(", "), null,
                                        actionIconTint, secondaryContainer, uriHandler
                                    )
                                }
                            }
                            detail.module.stars?.let { stars ->
                                add {
                                    InfoRowMiuix(
                                        stringResource(R.string.module_repo_stars),
                                        listOfNotNull(
                                            NumberFormat.getIntegerInstance().format(stars),
                                            detail.module.starsRepository,
                                        ).joinToString(" · "),
                                        starsUrl(detail.module.starsRepository),
                                        actionIconTint, secondaryContainer, uriHandler
                                    )
                                }
                            }
                            detail.license?.let { license ->
                                add {
                                    InfoRowMiuix(
                                        stringResource(R.string.module_repo_license), license, null,
                                        actionIconTint, secondaryContainer, uriHandler
                                    )
                                }
                            }
                            detail.homepage?.let { url ->
                                add {
                                    InfoRowMiuix(
                                        stringResource(R.string.module_repo_homepage), url, url,
                                        actionIconTint, secondaryContainer, uriHandler
                                    )
                                }
                            }
                            detail.support?.let { url ->
                                add {
                                    InfoRowMiuix(
                                        stringResource(R.string.module_repo_support), url, url,
                                        actionIconTint, secondaryContainer, uriHandler
                                    )
                                }
                            }
                            add {
                                InfoRowMiuix(
                                    stringResource(R.string.module_repo_integrity),
                                    stringResource(R.string.module_repo_integrity_desc),
                                    null, actionIconTint, secondaryContainer, uriHandler
                                )
                            }
                        }
                    )
                }
            }
            item { Spacer(Modifier.height(12.dp)) }
        }
    }
}

@SuppressLint("StringFormatInvalid", "DefaultLocale")
@Composable
fun ModuleRepoDetailScreenMiuix(
    state: ModuleRepoDetailUiState,
    actions: ModuleRepoDetailActions,
) {
    val context = LocalContext.current
    val enableBlur = LocalEnableBlur.current
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isInDarkTheme()) 0.7f else 0.9f)
    val secondaryContainer = colorScheme.secondaryContainer.copy(alpha = 0.8f)
    val module = state.module
    val scope = rememberCoroutineScope()
    val confirmTitle = stringResource(R.string.module_install)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })

    val scrollBehavior = MiuixScrollBehavior()

    val backdrop = rememberBlurBackdrop(enableBlur)
    val blurActive = backdrop != null
    val detailBarColor = if (blurActive) Color.Transparent else colorScheme.surface

    val tabs = listOf(
        stringResource(R.string.tab_readme), stringResource(R.string.tab_releases), stringResource(R.string.tab_info)
    )
    val pagerState = rememberPagerState(initialPage = 0, pageCount = { tabs.size })
    val tabRowHeight = 40.dp
    val dynamicTopPadding = remember(scrollBehavior) {
        { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }
    val coroutineScope = rememberCoroutineScope()

    Scaffold(
        topBar = {
            BlurredBar(backdrop) {
                TopAppBar(
                    color = detailBarColor,
                    title = state.detail?.module?.moduleName ?: module.moduleName,
                    scrollBehavior = scrollBehavior,
                    navigationIcon = {
                        IconButton(
                            onClick = actions.onBack
                        ) {
                            val layoutDirection = LocalLayoutDirection.current
                            Icon(
                                modifier = Modifier.graphicsLayer {
                                    if (layoutDirection == LayoutDirection.Rtl) scaleX = -1f
                                }, imageVector = MiuixIcons.Back, contentDescription = null, tint = colorScheme.onSurface
                            )
                        }
                    }, actions = {
                        if (state.webUrl.isNotEmpty()) {
                            IconButton(
                                onClick = actions.onOpenWebUrl
                            ) {
                                Icon(
                                    imageVector = MiuixIcons.HorizontalSplit, contentDescription = null, tint = colorScheme.onBackground
                                )
                            }
                        }
                    }, bottomContent = {
                        Column(
                            modifier = Modifier
                                .padding(horizontal = 12.dp)
                                .padding(bottom = 6.dp)
                                .deferredTopPadding(dynamicTopPadding)
                        ) {
                            TabRow(
                                tabs = tabs,
                                selectedTabIndex = pagerState.currentPage,
                                onTabSelected = { index ->
                                    coroutineScope.launch {
                                        pagerState.animateScrollToPage(
                                            page = index,
                                            animationSpec = PagerNavigationSpringSpec,
                                        )
                                    }
                                },
                                colors = TabRowDefaults.tabRowColors(
                                    backgroundColor = if (blurActive) Color.Transparent else colorScheme.surface
                                ),
                                height = tabRowHeight,
                            )
                        }
                    })
            }
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal),
    ) { innerPadding ->
        val layoutDirection = LocalLayoutDirection.current
        HorizontalPager(
            state = pagerState,
            modifier = Modifier.fillMaxSize(),
            overscrollEffect = null,
        ) { page ->
            val innerPadding = PaddingValues(
                top = innerPadding.calculateTopPadding(),
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection),
                bottom = innerPadding.calculateBottomPadding() + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding()
            )

            when (page) {
                0 -> ReadmePage(
                    state = state,
                    onRetry = actions.onRetry,
                    innerPadding = innerPadding,
                    scrollBehavior = scrollBehavior,
                    backdrop = backdrop
                )

                1 -> ReleasesPage(
                    state = state,
                    onRetry = actions.onRetry,
                    innerPadding = innerPadding,
                    scrollBehavior = scrollBehavior,
                    backdrop = backdrop,
                    actionIconTint = actionIconTint,
                    secondaryContainer = secondaryContainer,
                    confirmTitle = confirmTitle,
                    confirmDialog = confirmDialog,
                    scope = scope,
                    onInstallModule = actions.onInstallModule,
                    context = context,
                    setPendingDownload = { pendingDownload = it },
                    uriHandler = remember(actions) {
                        object : UriHandler {
                            override fun openUri(uri: String) = actions.onOpenUrl(uri)
                        }
                    },
                )

                2 -> {
                    val uriHandler = remember(actions) {
                        object : UriHandler {
                            override fun openUri(uri: String) = actions.onOpenUrl(uri)
                        }
                    }
                    InfoPage(
                        state = state,
                        innerPadding = innerPadding,
                        scrollBehavior = scrollBehavior,
                        backdrop = backdrop,
                        actionIconTint = actionIconTint,
                        secondaryContainer = secondaryContainer,
                        uriHandler = uriHandler,
                    )
                }
            }
        }
    }
}
