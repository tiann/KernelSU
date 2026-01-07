package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
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
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.zIndex
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.GithubMarkdown
import me.weishu.kernelsu.ui.component.navigation.LocalSharedTransitionScope
import me.weishu.kernelsu.ui.component.navigation.MiuixDestinationsNavigator
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.sharedTransition.TransitionSource
import me.weishu.kernelsu.ui.component.sharedTransition.screenShareBounds
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.DownloadListener
import me.weishu.kernelsu.ui.util.module.fetchModuleDetail
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.TabRow
import top.yukonga.miuix.kmp.basic.TabRowDefaults
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.icon.extended.HorizontalSplit
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@SuppressLint("StringFormatInvalid", "DefaultLocale")
@Composable
@Destination<RootGraph>
fun ModuleRepoDetailScreen(
    navigator: MiuixDestinationsNavigator,
    animatedVisibilityScope: AnimatedVisibilityScope,
    module: RepoModuleArg
) {
    val context = LocalContext.current
    val sharedTransitionScope = LocalSharedTransitionScope.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val isDark = isInDarkTheme(prefs.getInt("color_mode", 0))
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isDark) 0.7f else 0.9f)
    val secondaryContainer = colorScheme.secondaryContainer.copy(alpha = 0.8f)
    val uriHandler = LocalUriHandler.current
    val scope = rememberCoroutineScope()
    val confirmTitle = stringResource(R.string.module_install)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })
    val onInstallModule: (Uri) -> Unit = { uri ->
        navigator.navigate(FlashScreenDestination(FlashIt.FlashModules(listOf(uri)))) {
            launchSingleTop = true
        }
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
    BackHandler {
        navigator.popBackStack()
    }

    Box(
        modifier = Modifier
            .screenShareBounds(
                key = module.moduleId,
                transitionSource = TransitionSource.LIST_CARD,
                sharedTransitionScope = sharedTransitionScope,
                animatedVisibilityScope = animatedVisibilityScope,
            )
        ,
    ) {

        Scaffold(
            topBar = {
                TopAppBar(
                    modifier = Modifier.hazeEffect(hazeState) {
                        style = hazeStyle
                        blurRadius = 30.dp
                        noiseFactor = 0f
                    },
                    color = Color.Transparent,
                    title = module.moduleName,
                    scrollBehavior = scrollBehavior,
                    navigationIcon = {
                        IconButton(
                            modifier = Modifier.padding(start = 16.dp),
                            onClick = {
                                navigator.popBackStack()
                            }
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
            Box(
                modifier = Modifier.fillMaxSize()
            ) {
                var userScrollEnabled by remember { mutableStateOf(true) }
                var animating by remember { mutableStateOf(false) }
                var animateJob by remember { mutableStateOf<Job?>(null) }
                var tabSelectedIndex by remember { mutableIntStateOf(pagerState.currentPage) }
                var lastRequestedIndex by remember { mutableIntStateOf(pagerState.currentPage) }
                LaunchedEffect(pagerState) {
                    snapshotFlow { pagerState.currentPage }.collectLatest { page ->
                        if (!animating) tabSelectedIndex = page
                    }
                }
                Column(
                    modifier = Modifier
                        .hazeEffect(hazeState) {
                            style = hazeStyle
                            blurRadius = 30.dp
                            noiseFactor = 0f
                        }
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
                        selectedTabIndex = tabSelectedIndex,
                        onTabSelected = { index ->
                            tabSelectedIndex = index
                            if (index == pagerState.currentPage) {
                                if (animateJob != null && lastRequestedIndex != index) {
                                    animateJob?.cancel()
                                    animateJob = null
                                    animating = false
                                    userScrollEnabled = true
                                }
                                lastRequestedIndex = index
                            } else {
                                if (animateJob != null && lastRequestedIndex == index) {
                                    // Already animating to the requested page
                                } else {
                                    animateJob?.cancel()
                                    animating = true
                                    userScrollEnabled = false
                                    val job = scope.launch {
                                        try {
                                            pagerState.animateScrollToPage(index)
                                        } finally {
                                            if (animateJob === this) {
                                                userScrollEnabled = true
                                                animating = false
                                                animateJob = null
                                            }
                                        }
                                    }
                                    animateJob = job
                                    lastRequestedIndex = index
                                }
                            }
                        },
                        colors = TabRowDefaults.tabRowColors(backgroundColor = Color.Transparent),
                        height = tabRowHeight,
                    )
                }
                HorizontalPager(
                    state = pagerState,
                    modifier = Modifier.fillMaxSize(),
                    beyondViewportPageCount = 3,
                    userScrollEnabled = userScrollEnabled,
                ) { page ->
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
            DownloadListener(context, onInstallModule)
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
