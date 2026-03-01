package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import android.os.Parcelable
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.EaseInOut
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
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
import androidx.compose.runtime.collectAsState
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
import androidx.core.content.edit
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigationevent.NavigationEventInfo
import androidx.navigationevent.compose.NavigationBackHandler
import androidx.navigationevent.compose.rememberNavigationEventState
import com.kyant.capsule.ContinuousRoundedRectangle
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.ConfirmDialogHandle
import me.weishu.kernelsu.ui.component.GithubMarkdown
import me.weishu.kernelsu.ui.component.SearchBox
import me.weishu.kernelsu.ui.component.SearchPager
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.isNetworkAvailable
import me.weishu.kernelsu.ui.util.module.fetchModuleDetail
import me.weishu.kernelsu.ui.viewmodel.ModuleRepoViewModel
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CircularProgressIndicator
import top.yukonga.miuix.kmp.basic.DropdownImpl
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
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
import top.yukonga.miuix.kmp.extra.SuperListPopup
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.icon.extended.FileDownloads
import top.yukonga.miuix.kmp.icon.extended.HorizontalSplit
import top.yukonga.miuix.kmp.icon.extended.Link
import top.yukonga.miuix.kmp.icon.extended.MoreCircle
import top.yukonga.miuix.kmp.icon.extended.TopDownloads
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic
import java.text.Collator
import java.util.Locale

@Parcelize
data class ReleaseAssetArg(
    val name: String,
    val downloadUrl: String,
    val size: Long,
    val downloadCount: Int
) : Parcelable

@Parcelize
data class ReleaseArg(
    val tagName: String,
    val name: String,
    val publishedAt: String,
    val assets: List<ReleaseAssetArg>,
    val descriptionHTML: String
) : Parcelable

@Parcelize
data class AuthorArg(
    val name: String,
    val link: String,
) : Parcelable

@Parcelize
data class RepoModuleArg(
    val moduleId: String,
    val moduleName: String,
    val authors: String,
    val authorsList: List<AuthorArg>,
    val latestRelease: String,
    val latestReleaseTime: String,
    val releases: List<ReleaseArg>
) : Parcelable

@Composable
fun ModuleRepoScreen(
) {
    val navigator = LocalNavigator.current
    val viewModel = viewModel<ModuleRepoViewModel>()
    val uiState by viewModel.uiState.collectAsState()
    val installedVm = viewModel<ModuleViewModel>()
    val installedUiState by installedVm.uiState.collectAsState()
    val searchStatus = uiState.searchStatus
    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val metaBg = colorScheme.tertiaryContainer.copy(alpha = 0.6f)
    val metaTint = colorScheme.onTertiaryContainer.copy(alpha = 0.8f)
    val repoSortByNameState = remember { mutableStateOf(prefs.getBoolean("module_repo_sort_name", false)) }

    LaunchedEffect(Unit) {
        if (uiState.modules.isEmpty()) {
            viewModel.refresh()
        }
        if (installedUiState.moduleList.isEmpty()) {
            installedVm.fetchModuleList()
        }
    }

    LaunchedEffect(searchStatus.searchText) {
        viewModel.updateSearchText(searchStatus.searchText)
    }

    val scrollBehavior = MiuixScrollBehavior()
    val dynamicTopPadding by remember {
        derivedStateOf { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }

    val enableBlur = LocalEnableBlur.current
    val hazeState = remember { HazeState() }
    val hazeStyle = if (enableBlur) {
        HazeStyle(
            backgroundColor = colorScheme.surface,
            tint = HazeTint(colorScheme.surface.copy(0.8f))
        )
    } else {
        HazeStyle.Unspecified
    }

    Scaffold(
        topBar = {
            searchStatus.TopAppBarAnim(hazeState = hazeState, hazeStyle = hazeStyle) {
                TopAppBar(
                    color = if (enableBlur) Color.Transparent else colorScheme.surface,
                    title = stringResource(R.string.module_repos),
                    actions = {
                        val showTopPopup = remember { mutableStateOf(false) }
                        SuperListPopup(
                            show = showTopPopup,
                            popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                            alignment = PopupPositionProvider.Align.TopEnd,
                            onDismissRequest = { showTopPopup.value = false }
                        ) {
                            ListPopupColumn {
                                DropdownImpl(
                                    text = stringResource(R.string.module_repos_sort_name),
                                    optionSize = 1,
                                    isSelected = repoSortByNameState.value,
                                    onSelectedIndexChange = {
                                        repoSortByNameState.value = !repoSortByNameState.value
                                        prefs.edit {
                                            putBoolean("module_repo_sort_name", repoSortByNameState.value)
                                        }
                                        showTopPopup.value = false
                                    },
                                    index = 0
                                )
                            }
                        }
                        IconButton(
                            modifier = Modifier.padding(end = 16.dp),
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
                            modifier = Modifier.padding(start = 16.dp),
                            onClick = { navigator.pop() }

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
                    scrollBehavior = scrollBehavior,
                )
            }
        },
        popupHost = {
            searchStatus.SearchPager(
                onSearchStatusChange = viewModel::updateSearchStatus,
                defaultResult = {},
                searchBarTopPadding = dynamicTopPadding,
            ) {
                item {
                    Spacer(Modifier.height(6.dp))
                }
                val displaySearch = run {
                    val base = uiState.searchResults
                    val sortByName = repoSortByNameState.value
                    val collator = Collator.getInstance(Locale.getDefault())
                    if (!sortByName) base else base.sortedWith(compareBy(collator) { it.moduleName })
                }
                items(displaySearch, key = { it.moduleId }) { module ->
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 12.dp)
                            .padding(bottom = 12.dp),
                        insideMargin = PaddingValues(16.dp),
                        showIndication = true,
                        pressFeedbackType = PressFeedbackType.Sink,
                        onClick = {
                            val args = RepoModuleArg(
                                moduleId = module.moduleId,
                                moduleName = module.moduleName,
                                authors = module.authors,
                                authorsList = module.authorList.map { AuthorArg(it.name, it.link) },
                                latestRelease = module.latestRelease,
                                latestReleaseTime = module.latestReleaseTime,
                                releases = emptyList()
                            )
                            navigator.push(Route.ModuleRepoDetail(args))
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
                                                .clip(ContinuousRoundedRectangle(6.dp))
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
        },
    ) { innerPadding ->
        val layoutDirection = LocalLayoutDirection.current
        val isLoading = uiState.modules.isEmpty()
        val offline = !isNetworkAvailable(context)

        if (isLoading) {
            Box(
                modifier = Modifier
                    .fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                if (offline) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(text = stringResource(R.string.network_offline), color = colorScheme.onSurfaceVariantSummary, fontSize = 16.sp)
                        Spacer(Modifier.height(12.dp))
                        TextButton(
                            modifier = Modifier
                                .padding(horizontal = 24.dp)
                                .fillMaxWidth(),
                            text = stringResource(R.string.network_retry),
                            onClick = { viewModel.refresh() },
                        )
                    }
                } else {
                    InfiniteProgressIndicator()
                }
            }
        } else {
            searchStatus.SearchBox(
                onSearchStatusChange = viewModel::updateSearchStatus,
                searchBarTopPadding = dynamicTopPadding,
                contentPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding(),
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection)
                ),
                hazeState = hazeState,
                hazeStyle = hazeStyle
            ) { boxHeight ->
                val pullToRefreshState = rememberPullToRefreshState()
                val refreshTexts = listOf(
                    stringResource(R.string.refresh_pulling),
                    stringResource(R.string.refresh_release),
                    stringResource(R.string.refresh_refresh),
                    stringResource(R.string.refresh_complete),
                )
                PullToRefresh(
                    isRefreshing = uiState.isRefreshing,
                    pullToRefreshState = pullToRefreshState,
                    onRefresh = { viewModel.refresh() },
                    refreshTexts = refreshTexts,
                    contentPadding = PaddingValues(
                        top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection)
                    ),
                ) {
                    val displayModules = run {
                        val base = uiState.modules
                        val sortByName = repoSortByNameState.value
                        val collator = Collator.getInstance(Locale.getDefault())
                        if (!sortByName) base else base.sortedWith(compareBy(collator) { it.moduleName })
                    }
                    LazyColumn(
                        modifier = Modifier
                            .fillMaxHeight()
                            .scrollEndHaptic()
                            .overScrollVertical()
                            .nestedScroll(scrollBehavior.nestedScrollConnection)
                            .let { if (enableBlur) it.hazeSource(state = hazeState) else it },
                        contentPadding = PaddingValues(
                            top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                            start = innerPadding.calculateStartPadding(layoutDirection),
                            end = innerPadding.calculateEndPadding(layoutDirection)
                        ),
                        overscrollEffect = null,
                    ) {
                        items(
                            items = displayModules,
                            key = { it.moduleId },
                            contentType = { "module" }
                        ) { module ->
                            val moduleAuthor = stringResource(id = R.string.module_author)

                            Card(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 12.dp)
                                    .padding(bottom = 12.dp),
                                insideMargin = PaddingValues(16.dp),
                                showIndication = true,
                                pressFeedbackType = PressFeedbackType.Sink,
                                onClick = {
                                    val args = RepoModuleArg(
                                        moduleId = module.moduleId,
                                        moduleName = module.moduleName,
                                        authors = module.authors,
                                        authorsList = module.authorList.map { AuthorArg(it.name, it.link) },
                                        latestRelease = module.latestRelease,
                                        latestReleaseTime = module.latestReleaseTime,
                                        releases = emptyList()
                                    )
                                    navigator.push(Route.ModuleRepoDetail(args))
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
                                                        .clip(ContinuousRoundedRectangle(6.dp))
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

@Composable
private fun ReadmePage(
    readmeHtml: String?,
    readmeLoaded: Boolean,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    hazeState: HazeState
) {
    val layoutDirection = LocalLayoutDirection.current
    LazyColumn(
        modifier = Modifier
            .fillMaxHeight()
            .scrollEndHaptic()
            .overScrollVertical()
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .hazeSource(state = hazeState),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
            start = innerPadding.calculateStartPadding(layoutDirection),
            end = innerPadding.calculateEndPadding(layoutDirection),
            bottom = innerPadding.calculateBottomPadding(),
        ),
        overscrollEffect = null,
    ) {
        item {
            val isLoading = remember { mutableStateOf(true) }
            if (isLoading.value) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(
                            top = innerPadding.calculateTopPadding(),
                            start = innerPadding.calculateStartPadding(layoutDirection),
                            end = innerPadding.calculateEndPadding(layoutDirection),
                            bottom = innerPadding.calculateBottomPadding(),
                        ),
                    contentAlignment = Alignment.Center
                ) {
                    InfiniteProgressIndicator()
                }
            }
            AnimatedVisibility(
                visible = readmeLoaded && readmeHtml != null,
                enter = expandVertically() + fadeIn(),
                exit = shrinkVertically() + fadeOut()
            ) {
                Column {
                    Spacer(Modifier.height(6.dp))
                    Card(
                        modifier = Modifier.padding(horizontal = 12.dp),
                    ) {
                        Column {
                            GithubMarkdown(content = readmeHtml!!, isLoading)
                        }
                    }
                }
            }
        }
        item { Spacer(Modifier.height(12.dp)) }
    }
}

@SuppressLint("DefaultLocale")
@Composable
fun ReleasesPage(
    detailReleases: List<ReleaseArg>,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    hazeState: HazeState,
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
    LazyColumn(
        modifier = Modifier
            .fillMaxHeight()
            .scrollEndHaptic()
            .overScrollVertical()
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .hazeSource(state = hazeState),
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
            items(
                items = detailReleases,
                key = { it.tagName },
                contentType = { "release" }
            ) { rel ->
                val title = remember(rel.name, rel.tagName) { rel.name.ifBlank { rel.tagName } }
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 12.dp)
                        .padding(bottom = 12.dp)
                ) {
                    Column {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Column(
                                modifier = Modifier
                                    .padding(start = 16.dp, end = 16.dp, top = 16.dp)
                                    .weight(1f)
                            ) {
                                Text(
                                    text = title,
                                    fontSize = 17.sp,
                                    fontWeight = FontWeight(550),
                                    color = colorScheme.onSurface
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
                                    val onClickDownload = remember(fileName, asset.downloadUrl) {
                                        {
                                            val startText = context.getString(R.string.module_start_downloading, fileName)
                                            setPendingDownload {
                                                isDownloading = true
                                                scope.launch(Dispatchers.IO) {
                                                    download(
                                                        asset.downloadUrl,
                                                        fileName,
                                                        onDownloaded = onInstallModule,
                                                        onDownloading = { isDownloading = true },
                                                        onProgress = { p -> scope.launch(Dispatchers.Main) { progress = p } }
                                                    )
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
                                                text = fileName,
                                                fontSize = 14.sp,
                                                color = colorScheme.onSurface
                                            )
                                            Text(
                                                text = sizeAndDownloads,
                                                fontSize = 12.sp,
                                                color = colorScheme.onSurfaceVariantSummary,
                                                modifier = Modifier.padding(top = 2.dp)
                                            )
                                        }
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
                                                    progress = progress / 100f,
                                                    size = 20.dp,
                                                    strokeWidth = 2.dp
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

@Composable
fun InfoPage(
    module: RepoModuleArg,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    hazeState: HazeState,
    actionIconTint: Color,
    secondaryContainer: Color,
    uriHandler: UriHandler,
    sourceUrl: String,
) {
    val layoutDirection = LocalLayoutDirection.current
    LazyColumn(
        modifier = Modifier
            .fillMaxHeight()
            .scrollEndHaptic()
            .overScrollVertical()
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .hazeSource(state = hazeState),
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
                    text = stringResource(R.string.module_author),
                    modifier = Modifier.padding(top = 6.dp)
                )
                Card(
                    modifier = Modifier
                        .padding(horizontal = 12.dp),
                    insideMargin = PaddingValues(16.dp)
                ) {
                    Column {
                        module.authorsList.forEachIndexed { index, author ->
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                Text(
                                    text = author.name,
                                    fontSize = 14.sp,
                                    color = colorScheme.onSurface,
                                    modifier = Modifier.weight(1f)
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
                    text = stringResource(R.string.module_repos_source_code),
                    modifier = Modifier.padding(top = 6.dp)
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

@SuppressLint("StringFormatInvalid", "DefaultLocale")
@Composable
fun ModuleRepoDetailScreen(
    module: RepoModuleArg
) {
    val context = LocalContext.current
    val navigator = LocalNavigator.current
    val enableBlur = LocalEnableBlur.current
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isInDarkTheme()) 0.7f else 0.9f)
    val secondaryContainer = colorScheme.secondaryContainer.copy(alpha = 0.8f)
    val uriHandler = LocalUriHandler.current
    val scope = rememberCoroutineScope()
    val confirmTitle = stringResource(R.string.module_install)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })
    val onInstallModule: (Uri) -> Unit = { uri ->
        navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri))))
    }

    var readmeHtml by remember(module.moduleId) { mutableStateOf<String?>(null) }
    var readmeLoaded by remember(module.moduleId) { mutableStateOf(false) }
    var detailReleases by remember(module.moduleId) { mutableStateOf<List<ReleaseArg>>(emptyList()) }
    var webUrl by remember(module.moduleId) { mutableStateOf("https://modules.kernelsu.org/module/${module.moduleId}") }
    var sourceUrl by remember(module.moduleId) { mutableStateOf("https://github.com/KernelSU-Modules-Repo/${module.moduleId}") }

    val scrollBehavior = MiuixScrollBehavior()

    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = colorScheme.surface,
        tint = HazeTint(colorScheme.surface.copy(0.8f))
    )

    Scaffold(
        topBar = {
            TopAppBar(
                modifier = if (enableBlur) {
                    Modifier.hazeEffect(hazeState) {
                        style = hazeStyle
                        blurRadius = 30.dp
                        noiseFactor = 0f
                    }
                } else Modifier,
                color = if (enableBlur) Color.Transparent else colorScheme.surface,
                title = module.moduleName,
                scrollBehavior = scrollBehavior,
                navigationIcon = {
                    IconButton(
                        modifier = Modifier.padding(start = 16.dp),
                        onClick = { navigator.pop() }
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
                actions = {
                    if (webUrl.isNotEmpty()) {
                        IconButton(
                            modifier = Modifier.padding(end = 16.dp),
                            onClick = { uriHandler.openUri(webUrl) }
                        ) {
                            Icon(
                                imageVector = MiuixIcons.HorizontalSplit,
                                contentDescription = null,
                                tint = colorScheme.onBackground
                            )
                        }
                    }
                }
            )
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal),
    ) { innerPadding ->
        LaunchedEffect(module.moduleId) {
            if (module.moduleId.isNotEmpty()) {
                withContext(Dispatchers.IO) {
                    runCatching {
                        val detail = fetchModuleDetail(module.moduleId)
                        if (detail != null) {
                            readmeHtml = detail.readmeHtml
                            if (detail.sourceUrl.isNotEmpty()) sourceUrl = detail.sourceUrl
                            detailReleases = detail.releases.map { r ->
                                ReleaseArg(
                                    tagName = r.tagName,
                                    name = r.name,
                                    publishedAt = r.publishedAt,
                                    assets = r.assets.map { a -> ReleaseAssetArg(a.name, a.downloadUrl, a.size, a.downloadCount) },
                                    descriptionHTML = r.descriptionHTML
                                )
                            }
                        } else {
                            detailReleases = emptyList()
                        }
                    }.onSuccess {
                        readmeLoaded = true
                    }.onFailure {
                        readmeLoaded = true
                    }
                }
            } else {
                readmeLoaded = true
            }
        }
        val tabs = listOf(
            stringResource(R.string.tab_readme),
            stringResource(R.string.tab_releases),
            stringResource(R.string.tab_info)
        )
        val pagerState = rememberPagerState(initialPage = 0, pageCount = { tabs.size })
        LocalDensity.current
        val tabRowHeight by remember { mutableStateOf(40.dp) }
        var collapsedFraction by remember { mutableFloatStateOf(scrollBehavior.state.collapsedFraction) }
        LaunchedEffect(scrollBehavior.state.collapsedFraction) {
            snapshotFlow { scrollBehavior.state.collapsedFraction }.collectLatest { collapsedFraction = it }
        }
        val dynamicTopPadding by remember { derivedStateOf { 12.dp * (1f - collapsedFraction) } }
        val layoutDirection = LocalLayoutDirection.current
        val coroutineScope = rememberCoroutineScope()
        Box(
            modifier = Modifier.fillMaxSize()
        ) {
            Column(
                modifier = Modifier
                    .then(
                        if (enableBlur) {
                            Modifier.hazeEffect(hazeState) {
                                style = hazeStyle
                                blurRadius = 30.dp
                                noiseFactor = 0f
                            }
                        } else Modifier.background(colorScheme.surface),
                    )
                    .zIndex(1f)
                    .padding(
                        top = innerPadding.calculateTopPadding() + dynamicTopPadding,
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection),
                        bottom = 6.dp
                    )
                    .padding(horizontal = 12.dp)
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
                        backgroundColor = if (enableBlur) Color.Transparent else colorScheme.surface
                    ),
                    height = tabRowHeight,
                )
            }
            HorizontalPager(
                state = pagerState,
                modifier = Modifier.fillMaxSize(),
                beyondViewportPageCount = 2,
            ) { page ->
                run {
                    val navEventState = rememberNavigationEventState(NavigationEventInfo.None)
                    NavigationBackHandler(
                        state = navEventState,
                        isBackEnabled = pagerState.currentPage != 0,
                        onBackCompleted = {
                            scope.launch { pagerState.animateScrollToPage(0) }
                        }
                    )
                }
                val innerPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding() + tabRowHeight + dynamicTopPadding + 6.dp,
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection),
                    bottom = innerPadding.calculateBottomPadding() + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding()
                )

                when (page) {
                    0 -> ReadmePage(
                        readmeHtml = readmeHtml,
                        readmeLoaded = readmeLoaded,
                        innerPadding = innerPadding,
                        scrollBehavior = scrollBehavior,
                        hazeState = hazeState
                    )

                    1 -> ReleasesPage(
                        detailReleases = detailReleases,
                        innerPadding = innerPadding,
                        scrollBehavior = scrollBehavior,
                        hazeState = hazeState,
                        actionIconTint = actionIconTint,
                        secondaryContainer = secondaryContainer,
                        confirmTitle = confirmTitle,
                        confirmDialog = confirmDialog,
                        scope = scope,
                        onInstallModule = onInstallModule,
                        context = context,
                        setPendingDownload = { pendingDownload = it }
                    )

                    2 -> InfoPage(
                        module = module,
                        innerPadding = innerPadding,
                        scrollBehavior = scrollBehavior,
                        hazeState = hazeState,
                        actionIconTint = actionIconTint,
                        secondaryContainer = secondaryContainer,
                        uriHandler = uriHandler,
                        sourceUrl = sourceUrl,
                    )
                }
            }
        }
    }
}
