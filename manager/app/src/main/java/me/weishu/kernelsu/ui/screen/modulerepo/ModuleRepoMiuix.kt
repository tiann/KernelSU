package me.weishu.kernelsu.ui.screen.modulerepo

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.EaseInOut
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
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
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
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
import androidx.compose.ui.platform.LocalLocale
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.GithubMarkdown
import me.weishu.kernelsu.ui.component.ListPopupDefaults
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.component.dialog.ConfirmDialogHandle
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.miuix.SearchBarFake
import me.weishu.kernelsu.ui.component.miuix.SearchBox
import me.weishu.kernelsu.ui.component.miuix.SearchPager
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.rememberBlurBackdrop
import me.weishu.kernelsu.ui.util.rememberContentReady
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
import top.yukonga.miuix.kmp.icon.extended.MoreCircle
import top.yukonga.miuix.kmp.icon.extended.TopDownloads
import top.yukonga.miuix.kmp.overlay.OverlayListPopup
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.theme.miuixShape
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic
import java.text.Collator

@SuppressLint("LocalContextGetResourceValueCall")
@Composable
fun ModuleRepoScreenMiuix(
    state: ModuleRepoUiState,
    actions: ModuleRepoActions,
) {
    val searchStatus = state.searchStatus
    val density = LocalDensity.current
    val platformLocale = LocalLocale.current.platformLocale
    val metaBg = colorScheme.tertiaryContainer.copy(alpha = 0.6f)
    val metaTint = colorScheme.onTertiaryContainer.copy(alpha = 0.8f)

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
                            val showTopPopup = remember { mutableStateOf(false) }
                            OverlayListPopup(
                                show = showTopPopup.value, popupPositionProvider = ListPopupDefaults.MenuPositionProvider,
                                alignment = PopupPositionProvider.Align.TopEnd,
                                onDismissRequest = { showTopPopup.value = false },
                                content = {
                                    ListPopupColumn {
                                        DropdownImpl(
                                            text = stringResource(R.string.module_repos_sort_name),
                                            optionSize = 1,
                                            isSelected = state.sortByName,
                                            onSelectedIndexChange = {
                                                actions.onToggleSortByName()
                                                showTopPopup.value = false
                                            },
                                            index = 0
                                        )
                                    }
                                })
                            IconButton(
                                onClick = { showTopPopup.value = true },
                                holdDownState = showTopPopup.value
                            ) {
                                Icon(
                                    imageVector = MiuixIcons.MoreCircle,
                                    tint = colorScheme.onSurface,
                                    contentDescription = null,
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
                val displaySearch = remember(state.searchResults, state.sortByName) {
                    val collator = Collator.getInstance(platformLocale)
                    if (!state.sortByName) state.searchResults else state.searchResults.sortedWith(compareBy(collator) { it.moduleName })
                }
                LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .overScrollVertical(),
                ) {
                    item {
                        Spacer(Modifier.height(6.dp))
                    }
                    items(displaySearch, key = { it.moduleId }, contentType = { "module" }) { module ->
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
                                        Text(
                                            text = module.moduleName,
                                            fontSize = 17.sp,
                                            fontWeight = FontWeight(550),
                                            color = colorScheme.onSurface
                                        )
                                        if (module.metamodule) {
                                            Text(
                                                text = "META",
                                                fontSize = 12.sp,
                                                color = metaTint,
                                                modifier = Modifier
                                                    .padding(start = 6.dp)
                                                    .clip(miuixShape(6.dp))
                                                    .background(metaBg)
                                                    .padding(horizontal = 6.dp, vertical = 2.dp),
                                                fontWeight = FontWeight(750),
                                                maxLines = 1
                                            )
                                        }
                                        Spacer(Modifier.weight(1f))
                                        if (module.stargazerCount > 0) {
                                            Row(verticalAlignment = Alignment.CenterVertically) {
                                                Icon(
                                                    imageVector = MiuixIcons.TopDownloads,
                                                    contentDescription = "stars",
                                                    tint = colorScheme.onSurfaceVariantSummary,
                                                    modifier = Modifier.size(16.dp)
                                                )
                                                Text(
                                                    text = module.stargazerCount.toString(),
                                                    fontSize = 12.sp,
                                                    color = colorScheme.onSurfaceVariantSummary,
                                                    modifier = Modifier.padding(start = 4.dp)
                                                )
                                            }
                                        }
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
        val offline = state.offline

        searchStatus.SearchBox(
            onSearchStatusChange = actions.onSearchStatusChange,
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
                    if (offline) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            Text(
                                text = stringResource(R.string.network_offline),
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
                    } else {
                        InfiniteProgressIndicator()
                    }
                }
            }
            if (!isLoading && contentReady) {
                val pullToRefreshState = rememberPullToRefreshState()
                val refreshTexts = listOf(
                    stringResource(R.string.refresh_pulling),
                    stringResource(R.string.refresh_release),
                    stringResource(R.string.refresh_refresh),
                    stringResource(R.string.refresh_complete),
                )
                PullToRefresh(
                    isRefreshing = state.isRefreshing,
                    pullToRefreshState = pullToRefreshState,
                    onRefresh = actions.onRefresh,
                    refreshTexts = refreshTexts,
                    contentPadding = PaddingValues(
                        top = innerPadding.calculateTopPadding() + 6.dp,
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection)
                    ),
                ) {
                    val displayModules = remember(state.modules, state.sortByName) {
                        val collator = Collator.getInstance(platformLocale)
                        if (!state.sortByName) state.modules else state.modules.sortedWith(compareBy(collator) { it.moduleName })
                    }
                    Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
                        LazyColumn(
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
                            items(items = displayModules, key = { it.moduleId }, contentType = { "module" }) { module ->
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
                                                Text(
                                                    text = module.moduleName,
                                                    fontSize = 17.sp,
                                                    fontWeight = FontWeight(550),
                                                    color = colorScheme.onSurface
                                                )
                                                if (module.metamodule) {
                                                    Text(
                                                        text = "META",
                                                        fontSize = 12.sp,
                                                        color = metaTint,
                                                        modifier = Modifier
                                                            .padding(start = 6.dp)
                                                            .clip(miuixShape(6.dp))
                                                            .background(metaBg)
                                                            .padding(horizontal = 6.dp, vertical = 2.dp),
                                                        fontWeight = FontWeight(750),
                                                        maxLines = 1
                                                    )
                                                }
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
                                                if (module.stargazerCount > 0) {
                                                    Row(verticalAlignment = Alignment.CenterVertically) {
                                                        Icon(
                                                            imageVector = MiuixIcons.TopDownloads,
                                                            contentDescription = "stars",
                                                            tint = colorScheme.onSurfaceVariantSummary,
                                                            modifier = Modifier.size(16.dp)
                                                        )
                                                        Text(
                                                            text = module.stargazerCount.toString(),
                                                            fontSize = 12.sp,
                                                            color = colorScheme.onSurfaceVariantSummary,
                                                            modifier = Modifier.padding(start = 4.dp)
                                                        )
                                                    }
                                                }
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

@Composable
private fun ReadmePage(
    readmeHtml: String?,
    readmeLoaded: Boolean,
    innerPadding: PaddingValues, scrollBehavior: ScrollBehavior, backdrop: LayerBackdrop?
) {
    val layoutDirection = LocalLayoutDirection.current
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
                val contentReady = rememberContentReady()
                var isLoading by remember { mutableStateOf(true) }
                if (isLoading) {
                    Box(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(
                                top = innerPadding.calculateTopPadding(),
                                start = innerPadding.calculateStartPadding(layoutDirection),
                                end = innerPadding.calculateEndPadding(layoutDirection),
                                bottom = innerPadding.calculateBottomPadding(),
                            ), contentAlignment = Alignment.Center
                    ) {
                        InfiniteProgressIndicator()
                    }
                }
                AnimatedVisibility(
                    visible = contentReady && readmeLoaded && readmeHtml != null,
                    enter = expandVertically() + fadeIn(),
                    exit = shrinkVertically() + fadeOut()
                ) {
                    Column {
                        Spacer(Modifier.height(6.dp))
                        Card(
                            modifier = Modifier.padding(horizontal = 12.dp),
                        ) {
                            GithubMarkdown(content = readmeHtml!!, onLoadingChange = { isLoading = it })
                        }
                    }
                }
            }
            item { Spacer(Modifier.height(12.dp)) }
        }
    }
}

@SuppressLint("DefaultLocale")
@Composable
fun ReleasesPage(
    detailReleases: List<ReleaseArg>,
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
) {
    val layoutDirection = LocalLayoutDirection.current
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
            if (detailReleases.isNotEmpty()) {
                item {
                    Spacer(Modifier.height(6.dp))
                }
                items(items = detailReleases, key = { it.tagName }, contentType = { "release" }) { rel ->
                    val title = remember(rel.name, rel.tagName) { rel.name.ifBlank { rel.tagName } }
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
                                        text = title, fontSize = 17.sp, fontWeight = FontWeight(550), color = colorScheme.onSurface
                                    )
                                    Text(
                                        text = rel.tagName,
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
                            AnimatedVisibility(
                                visible = rel.assets.isNotEmpty(),
                                enter = fadeIn() + expandVertically(),
                                exit = fadeOut() + shrinkVertically()
                            ) {
                                Column {
                                    AnimatedVisibility(
                                        visible = rel.descriptionHTML.isNotEmpty(),
                                        enter = fadeIn() + expandVertically(),
                                        exit = fadeOut() + shrinkVertically()
                                    ) {
                                        Column {
                                            HorizontalDivider(
                                                modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 4.dp),
                                                thickness = 0.5.dp,
                                                color = colorScheme.outline.copy(alpha = 0.5f)
                                            )
                                            GithubMarkdown(content = rel.descriptionHTML)
                                        }
                                    }
                                    HorizontalDivider(
                                        modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 8.dp),
                                        thickness = 0.5.dp,
                                        color = colorScheme.outline.copy(alpha = 0.5f)
                                    )
                                    rel.assets.forEachIndexed { index, asset ->
                                        val fileName = asset.name
                                        stringResource(R.string.module_downloading)
                                        val sizeText = remember(asset.size) {
                                            val s = asset.size
                                            when {
                                                s >= 1024L * 1024L * 1024L -> String.format("%.1f GB", s / (1024f * 1024f * 1024f))
                                                s >= 1024L * 1024L -> String.format("%.1f MB", s / (1024f * 1024f))
                                                s >= 1024L -> String.format("%.0f KB", s / 1024f)
                                                else -> "$s B"
                                            }
                                        }
                                        val sizeAndDownloads =
                                            remember(sizeText, asset.downloadCount) { "$sizeText · ${asset.downloadCount} downloads" }
                                        var isDownloading by remember(fileName, asset.downloadUrl) { mutableStateOf(false) }
                                        var progress by remember(fileName, asset.downloadUrl) { mutableIntStateOf(0) }
                                        var downloadedUri by remember(fileName, asset.downloadUrl) { mutableStateOf<Uri?>(null) }
                                        val isDownloaded = downloadedUri != null
                                        val onClickDownload = remember(fileName, asset.downloadUrl) {
                                            {
                                                val startText = context.getString(R.string.module_start_downloading, fileName)
                                                setPendingDownload {
                                                    isDownloading = true
                                                    scope.launch(Dispatchers.IO) {
                                                        download(
                                                            asset.downloadUrl,
                                                            fileName,
                                                            onDownloaded = { uri ->
                                                                isDownloading = false
                                                                downloadedUri = uri
                                                            },
                                                            onDownloading = { isDownloading = true },
                                                            onProgress = { p -> scope.launch(Dispatchers.Main) { progress = p } })
                                                    }
                                                }
                                                confirmDialog.showConfirm(title = confirmTitle, content = startText)
                                            }
                                        }
                                        val bottomPadding = if (index == rel.assets.lastIndex) 16.dp else 8.dp
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            verticalAlignment = Alignment.CenterVertically,
                                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                                        ) {
                                            Column(
                                                modifier = Modifier
                                                    .padding(start = 16.dp, end = 16.dp, bottom = bottomPadding)
                                                    .weight(1f)
                                            ) {
                                                Text(
                                                    text = fileName, fontSize = 14.sp, color = colorScheme.onSurface
                                                )
                                                Text(
                                                    text = sizeAndDownloads,
                                                    fontSize = 12.sp,
                                                    color = colorScheme.onSurfaceVariantSummary,
                                                    modifier = Modifier.padding(top = 2.dp)
                                                )
                                            }
                                            if (isDownloaded) {
                                                IconButton(
                                                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = bottomPadding),
                                                    backgroundColor = secondaryContainer,
                                                    minHeight = 35.dp,
                                                    minWidth = 35.dp,
                                                    onClick = {
                                                        val uri = downloadedUri ?: return@IconButton
                                                        val file = uri.path?.let { java.io.File(it) }
                                                        if (file != null && file.exists()) {
                                                            onInstallModule(uri)
                                                        } else {
                                                            downloadedUri = null
                                                        }
                                                    },
                                                ) {
                                                    Row(
                                                        modifier = Modifier.padding(horizontal = 10.dp),
                                                        verticalAlignment = Alignment.CenterVertically,
                                                    ) {
                                                        Icon(
                                                            modifier = Modifier.size(20.dp),
                                                            imageVector = MiuixIcons.FileDownloads,
                                                            tint = actionIconTint,
                                                            contentDescription = stringResource(R.string.install)
                                                        )
                                                        Text(
                                                            modifier = Modifier.padding(start = 4.dp, end = 2.dp),
                                                            text = stringResource(R.string.install),
                                                            color = actionIconTint,
                                                            fontWeight = FontWeight.Medium,
                                                            fontSize = 15.sp
                                                        )
                                                    }
                                                }
                                            } else {
                                                IconButton(
                                                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = bottomPadding),
                                                    backgroundColor = secondaryContainer,
                                                    minHeight = 35.dp,
                                                    minWidth = 35.dp,
                                                    enabled = !isDownloading,
                                                    onClick = onClickDownload,
                                                ) {
                                                    if (isDownloading) {
                                                        CircularProgressIndicator(
                                                            progress = progress / 100f, size = 20.dp, strokeWidth = 2.dp
                                                        )
                                                    } else {
                                                        Row(
                                                            modifier = Modifier.padding(horizontal = 10.dp),
                                                            verticalAlignment = Alignment.CenterVertically,
                                                        ) {
                                                            Icon(
                                                                modifier = Modifier.size(20.dp),
                                                                imageVector = MiuixIcons.FileDownloads,
                                                                tint = actionIconTint,
                                                                contentDescription = stringResource(R.string.download)
                                                            )
                                                            Text(
                                                                modifier = Modifier.padding(start = 4.dp, end = 2.dp),
                                                                text = stringResource(R.string.download),
                                                                color = actionIconTint,
                                                                fontWeight = FontWeight.Medium,
                                                                fontSize = 15.sp
                                                            )
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if (index != rel.assets.lastIndex) {
                                            HorizontalDivider(
                                                modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 8.dp),
                                                thickness = 0.5.dp,
                                                color = colorScheme.outline.copy(alpha = 0.5f)
                                            )
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
fun InfoPage(
    module: RepoModuleArg,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    backdrop: LayerBackdrop?,
    actionIconTint: Color,
    secondaryContainer: Color,
    uriHandler: UriHandler,
    sourceUrl: String,
) {
    val layoutDirection = LocalLayoutDirection.current
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
            if (module.authorsList.isNotEmpty()) {
                item {
                    SmallTitle(
                        text = stringResource(R.string.module_author), modifier = Modifier.padding(top = 6.dp)
                    )
                    Card(
                        modifier = Modifier.padding(horizontal = 12.dp), insideMargin = PaddingValues(16.dp)
                    ) {
                        Column {
                            module.authorsList.forEachIndexed { index, author ->
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    Text(
                                        text = author.name, fontSize = 14.sp, color = colorScheme.onSurface, modifier = Modifier.weight(1f)
                                    )
                                    val clickable = author.link.isNotEmpty()
                                    val tint = if (clickable) actionIconTint else actionIconTint.copy(alpha = 0.35f)
                                    IconButton(
                                        backgroundColor = secondaryContainer,
                                        minHeight = 35.dp,
                                        minWidth = 35.dp,
                                        enabled = clickable,
                                        onClick = {
                                            if (clickable) {
                                                uriHandler.openUri(author.link)
                                            }
                                        },
                                    ) {
                                        Icon(
                                            modifier = Modifier.size(20.dp),
                                            imageVector = MiuixIcons.Link,
                                            tint = tint,
                                            contentDescription = null
                                        )
                                    }
                                }
                                if (index != module.authorsList.lastIndex) {
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
            }
            if (sourceUrl.isNotEmpty()) {
                item {
                    SmallTitle(
                        text = stringResource(R.string.module_repos_source_code), modifier = Modifier.padding(top = 6.dp)
                    )
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 12.dp)
                            .padding(bottom = 12.dp),
                        insideMargin = PaddingValues(16.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Text(
                                text = sourceUrl,
                                fontSize = 16.sp,
                                color = colorScheme.onSurface,
                                modifier = Modifier.weight(1f),
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                            IconButton(
                                backgroundColor = secondaryContainer,
                                minHeight = 35.dp,
                                minWidth = 35.dp,
                                onClick = {
                                    uriHandler.openUri(sourceUrl)
                                },
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
    val tabRowHeight by remember { mutableStateOf(40.dp) }
    var collapsedFraction by remember { mutableFloatStateOf(scrollBehavior.state.collapsedFraction) }
    LaunchedEffect(scrollBehavior.state.collapsedFraction) {
        snapshotFlow { scrollBehavior.state.collapsedFraction }.collectLatest { collapsedFraction = it }
    }
    val dynamicTopPadding by remember { derivedStateOf { 12.dp * (1f - collapsedFraction) } }
    val coroutineScope = rememberCoroutineScope()

    Scaffold(
        topBar = {
            BlurredBar(backdrop) {
                TopAppBar(color = detailBarColor, title = module.moduleName, scrollBehavior = scrollBehavior, navigationIcon = {
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
                            .padding(top = dynamicTopPadding, bottom = 6.dp)
                    ) {
                        TabRow(
                            tabs = tabs,
                            selectedTabIndex = pagerState.currentPage,
                            onTabSelected = { index ->
                                coroutineScope.launch {
                                    pagerState.animateScrollToPage(page = index, animationSpec = tween(easing = EaseInOut))
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
        ) { page ->
            val innerPadding = PaddingValues(
                top = innerPadding.calculateTopPadding(),
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection),
                bottom = innerPadding.calculateBottomPadding() + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding()
            )

            when (page) {
                0 -> ReadmePage(
                    readmeHtml = state.readmeHtml,
                    readmeLoaded = state.readmeLoaded,
                    innerPadding = innerPadding,
                    scrollBehavior = scrollBehavior,
                    backdrop = backdrop
                )

                1 -> ReleasesPage(
                    detailReleases = state.detailReleases,
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
                    setPendingDownload = { pendingDownload = it })

                2 -> {
                    val uriHandler = remember(actions) {
                        object : UriHandler {
                            override fun openUri(uri: String) = actions.onOpenUrl(uri)
                        }
                    }
                    InfoPage(
                        module = module,
                        innerPadding = innerPadding,
                        scrollBehavior = scrollBehavior,
                        backdrop = backdrop,
                        actionIconTint = actionIconTint,
                        secondaryContainer = secondaryContainer,
                        uriHandler = uriHandler,
                        sourceUrl = state.sourceUrl,
                    )
                }
            }
        }
    }
}
