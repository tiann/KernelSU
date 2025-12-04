package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import android.os.Parcelable
import androidx.compose.animation.AnimatedVisibility
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
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Link
import androidx.compose.material.icons.rounded.Star
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
import androidx.core.content.edit
import androidx.lifecycle.viewmodel.compose.viewModel
import com.kyant.capsule.ContinuousRoundedRectangle
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import com.ramcosta.composedestinations.generated.destinations.ModuleRepoDetailScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.ConfirmDialogHandle
import me.weishu.kernelsu.ui.component.GithubMarkdownContent
import me.weishu.kernelsu.ui.component.MarkdownContent
import me.weishu.kernelsu.ui.component.SearchBox
import me.weishu.kernelsu.ui.component.SearchPager
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.DownloadListener
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.isNetworkAvailable
import me.weishu.kernelsu.ui.util.module.UpdateState
import me.weishu.kernelsu.ui.util.module.compareVersionCode
import me.weishu.kernelsu.ui.util.module.fetchModuleDetail
import me.weishu.kernelsu.ui.util.module.fetchReleaseDescriptionHtml
import me.weishu.kernelsu.ui.viewmodel.ModuleRepoViewModel
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CircularProgressIndicator
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.ListPopup
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
import top.yukonga.miuix.kmp.extra.DropdownImpl
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.icon.icons.useful.ImmersionMore
import top.yukonga.miuix.kmp.icon.icons.useful.NavigatorSwitch
import top.yukonga.miuix.kmp.icon.icons.useful.Save
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.getWindowSize
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
@Destination<RootGraph>
fun ModuleRepoPager(
    navigator: DestinationsNavigator,
    bottomInnerPadding: Dp
) {
    val viewModel = viewModel<ModuleRepoViewModel>()
    val installedVm = viewModel<ModuleViewModel>()
    val searchStatus by viewModel.searchStatus
    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val isDark = isInDarkTheme(prefs.getInt("color_mode", 0))
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isDark) 0.7f else 0.9f)
    val updateBg = colorScheme.tertiaryContainer.copy(alpha = 0.6f)
    val updateTint = colorScheme.onTertiaryContainer.copy(alpha = 0.8f)
    val scope = rememberCoroutineScope()
    val repoSortUpdateFirstState = remember { mutableStateOf(prefs.getBoolean("module_repo_sort_update_first", true)) }
    val repoSortByNameState = remember { mutableStateOf(prefs.getBoolean("module_repo_sort_name", false)) }

    LaunchedEffect(Unit) {
        if (viewModel.modules.value.isEmpty()) {
            viewModel.refresh()
        }
        if (installedVm.moduleList.isEmpty()) {
            installedVm.fetchModuleList()
        }
    }

    val scrollBehavior = MiuixScrollBehavior()
    var collapsedFraction by remember { mutableFloatStateOf(scrollBehavior.state.collapsedFraction) }
    LaunchedEffect(scrollBehavior.state) {
        snapshotFlow { scrollBehavior.state.collapsedFraction }.collectLatest { collapsedFraction = it }
    }
    val dynamicTopPadding = 12.dp * (1f - collapsedFraction)

    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = colorScheme.surface,
        tint = HazeTint(colorScheme.surface.copy(0.8f))
    )

    val onInstallModule: (Uri) -> Unit = { uri ->
        navigator.navigate(FlashScreenDestination(FlashIt.FlashModules(listOf(uri)))) {
            launchSingleTop = true
        }
    }

    val confirmTitle = stringResource(R.string.module_install)
    val updateTitle = stringResource(R.string.module_update)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })

    Scaffold(
        topBar = {
            searchStatus.TopAppBarAnim(hazeState = hazeState, hazeStyle = hazeStyle) {
                TopAppBar(
                    color = Color.Transparent,
                    title = stringResource(R.string.module_repos),
                    actions = {
                        val showTopPopup = remember { mutableStateOf(false) }
                        ListPopup(
                            show = showTopPopup,
                            popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                            alignment = PopupPositionProvider.Align.TopRight,
                            onDismissRequest = { showTopPopup.value = false }
                        ) {
                            ListPopupColumn {
                                DropdownImpl(
                                    text = stringResource(R.string.module_repos_sort_update_first),
                                    optionSize = 2,
                                    isSelected = repoSortUpdateFirstState.value,
                                    onSelectedIndexChange = {
                                        repoSortUpdateFirstState.value = !repoSortUpdateFirstState.value
                                        prefs.edit {
                                            putBoolean("module_repo_sort_update_first", repoSortUpdateFirstState.value)
                                        }
                                        showTopPopup.value = false
                                    },
                                    index = 0
                                )
                                DropdownImpl(
                                    text = stringResource(R.string.module_repos_sort_name),
                                    optionSize = 2,
                                    isSelected = repoSortByNameState.value,
                                    onSelectedIndexChange = {
                                        repoSortByNameState.value = !repoSortByNameState.value
                                        prefs.edit {
                                            putBoolean("module_repo_sort_name", repoSortByNameState.value)
                                        }
                                        showTopPopup.value = false
                                    },
                                    index = 1
                                )
                            }
                        }
                        IconButton(
                            modifier = Modifier.padding(end = 16.dp),
                            onClick = { showTopPopup.value = true },
                            holdDownState = showTopPopup.value
                        ) {
                            Icon(
                                imageVector = MiuixIcons.Useful.ImmersionMore,
                                contentDescription = stringResource(id = R.string.settings),
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
                defaultResult = {},
                searchBarTopPadding = dynamicTopPadding,
            ) {
                item {
                    Spacer(Modifier.height(6.dp))
                }
                val displaySearch = run {
                    val base = viewModel.searchResults.value
                    val updateFirst = repoSortUpdateFirstState.value
                    val sortByName = repoSortByNameState.value
                    val collator = Collator.getInstance(Locale.getDefault())
                    val sortedBase = if (!sortByName) base else base.sortedWith(compareBy(collator) { it.moduleName })
                    if (!updateFirst) sortedBase else {
                        val (updatable, others) = sortedBase.partition { module ->
                            val installed = installedVm.moduleList.firstOrNull { it.id == module.moduleId }
                            compareVersionCode(installed?.versionCode, module.latestVersionCode) == UpdateState.CAN_UPDATE
                        }
                        val up = if (!sortByName) updatable else updatable.sortedWith(compareBy(collator) { it.moduleName })
                        val ot = if (!sortByName) others else others.sortedWith(compareBy(collator) { it.moduleName })
                        up + ot
                    }
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
                            navigator.navigate(ModuleRepoDetailScreenDestination(args)) { launchSingleTop = true }
                        }
                    ) {
                        Column {
                            if (module.moduleName.isNotBlank()) {
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
                                            color = updateTint,
                                            modifier = Modifier
                                                .padding(start = 6.dp)
                                                .clip(ContinuousRoundedRectangle(6.dp))
                                                .background(updateBg)
                                                .padding(horizontal = 6.dp, vertical = 2.dp),
                                            fontWeight = FontWeight(750),
                                            maxLines = 1
                                        )
                                    }
                                    Spacer(Modifier.weight(1f))
                                    if (module.stargazerCount > 0) {
                                        Row(verticalAlignment = Alignment.CenterVertically) {
                                            Icon(
                                                imageVector = Icons.Rounded.Star,
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
                            if (module.moduleId.isNotBlank()) {
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
                            if (module.summary.isNotBlank()) {
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
        val isLoading = viewModel.modules.value.isEmpty()
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
            LaunchedEffect(searchStatus.searchText) { viewModel.updateSearchText(searchStatus.searchText) }
            searchStatus.SearchBox(
                searchBarTopPadding = dynamicTopPadding,
                contentPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding(),
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection)
                ),
                hazeState = hazeState,
                hazeStyle = hazeStyle
            ) { boxHeight ->
                var isRefreshing by rememberSaveable { mutableStateOf(false) }
                val pullToRefreshState = rememberPullToRefreshState()
                LaunchedEffect(isRefreshing) {
                    if (isRefreshing) {
                        delay(450)
                        viewModel.refresh()
                        isRefreshing = false
                    }
                }
                val refreshTexts = listOf(

                    stringResource(R.string.refresh_pulling),
                    stringResource(R.string.refresh_release),
                    stringResource(R.string.refresh_refresh),
                    stringResource(R.string.refresh_complete),
                )
                PullToRefresh(
                    isRefreshing = isRefreshing,
                    pullToRefreshState = pullToRefreshState,
                    onRefresh = { if (!isRefreshing) isRefreshing = true },
                    refreshTexts = refreshTexts,
                    contentPadding = PaddingValues(
                        top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection)
                    ),
                ) {
                    val displayModules = run {
                        val base = viewModel.modules.value
                        val updateFirst = repoSortUpdateFirstState.value
                        val sortByName = repoSortByNameState.value
                        val collator = Collator.getInstance(Locale.getDefault())
                        val sortedBase = if (!sortByName) base else base.sortedWith(compareBy(collator) { it.moduleName })
                        if (!updateFirst) sortedBase else {
                            val (updatable, others) = sortedBase.partition { module ->
                                val installed = installedVm.moduleList.firstOrNull { it.id == module.moduleId }
                                compareVersionCode(installed?.versionCode, module.latestVersionCode) == UpdateState.CAN_UPDATE
                            }
                            val up = if (!sortByName) updatable else updatable.sortedWith(compareBy(collator) { it.moduleName })
                            val ot = if (!sortByName) others else others.sortedWith(compareBy(collator) { it.moduleName })
                            up + ot
                        }
                    }
                    LazyColumn(
                        modifier = Modifier
                            .height(getWindowSize().height.dp)
                            .scrollEndHaptic()
                            .overScrollVertical()
                            .nestedScroll(scrollBehavior.nestedScrollConnection)
                            .hazeSource(state = hazeState),
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
                            val latestTag = module.latestRelease
                            val latestAsset = module.latestAsset

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
                                    navigator.navigate(ModuleRepoDetailScreenDestination(args)) {
                                        launchSingleTop = true
                                    }
                                }
                            ) {
                                Column {
                                    if (module.moduleName.isNotBlank()) {
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
                                                    color = updateTint,
                                                    modifier = Modifier
                                                        .padding(start = 6.dp)
                                                        .clip(ContinuousRoundedRectangle(6.dp))
                                                        .background(updateBg)
                                                        .padding(horizontal = 6.dp, vertical = 2.dp),
                                                    fontWeight = FontWeight(750),
                                                    maxLines = 1
                                                )
                                            }
                                            Spacer(Modifier.weight(1f))
                                            if (module.stargazerCount > 0) {
                                                Row(verticalAlignment = Alignment.CenterVertically) {
                                                    Icon(
                                                        imageVector = Icons.Rounded.Star,
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
                                    if (module.moduleId.isNotBlank()) {
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
                                    if (module.summary.isNotBlank()) {
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
                                        Column {
                                            Text(
                                                text = latestTag,
                                                fontSize = 12.sp,
                                                modifier = Modifier.padding(top = 2.dp),
                                                fontWeight = FontWeight(550),
                                                color = colorScheme.onSurfaceVariantSummary,
                                            )
                                            if (module.latestReleaseTime.isNotBlank()) {
                                                Text(
                                                    text = module.latestReleaseTime,
                                                    fontSize = 12.sp,
                                                    color = colorScheme.onSurfaceVariantSummary,
                                                    textAlign = TextAlign.End
                                                )
                                            }
                                        }
                                        Spacer(Modifier.weight(1f))
                                        if (latestAsset != null) {
                                            val fileName = latestAsset.name
                                            stringResource(R.string.module_downloading)
                                            var isDownloading by remember(fileName, latestAsset.downloadUrl) { mutableStateOf(false) }
                                            var progress by remember(fileName, latestAsset.downloadUrl) { mutableIntStateOf(0) }
                                            val installed = installedVm.moduleList.firstOrNull { it.id == module.moduleId }
                                            val repoCode = module.latestVersionCode
                                            val state = compareVersionCode(installed?.versionCode, repoCode)
                                            val canUpdateByCode = state == UpdateState.CAN_UPDATE
                                            val equalByCode = state == UpdateState.EQUAL
                                            val olderByCode = state == UpdateState.OLDER
                                            IconButton(
                                                backgroundColor = if (canUpdateByCode) updateBg else colorScheme.secondaryContainer.copy(
                                                    alpha = 0.8f
                                                ),
                                                minHeight = 35.dp,
                                                minWidth = 35.dp,
                                                enabled = !isDownloading && !olderByCode,
                                                onClick = {
                                                    pendingDownload = {
                                                        isDownloading = true
                                                        scope.launch(Dispatchers.IO) {
                                                            download(
                                                                latestAsset.downloadUrl,
                                                                fileName,
                                                                onDownloaded = onInstallModule,
                                                                onDownloading = { isDownloading = true },
                                                                onProgress = { p -> scope.launch(Dispatchers.Main) { progress = p } }
                                                            )
                                                        }
                                                    }
                                                    val startDownloadingText =
                                                        context.getString(R.string.module_start_downloading, fileName)
                                                    if (canUpdateByCode) {
                                                        var confirmContent = startDownloadingText
                                                        var confirmHtml = false
                                                        scope.launch(Dispatchers.IO) {
                                                            runCatching {
                                                                val html = fetchReleaseDescriptionHtml(module.moduleId, latestTag)
                                                                if (html != null) {
                                                                    confirmContent = html
                                                                    confirmHtml = true
                                                                }
                                                            }.onSuccess {
                                                                withContext(Dispatchers.Main) {
                                                                    confirmDialog.showConfirm(
                                                                        title = updateTitle,
                                                                        content = confirmContent,
                                                                        html = confirmHtml
                                                                    )
                                                                }
                                                            }.onFailure {
                                                                withContext(Dispatchers.Main) {
                                                                    confirmDialog.showConfirm(
                                                                        title = confirmTitle,
                                                                        content = startDownloadingText
                                                                    )
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        confirmDialog.showConfirm(
                                                            title = confirmTitle,
                                                            content = startDownloadingText
                                                        )
                                                    }
                                                },
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
                                                            imageVector = MiuixIcons.Useful.Save,
                                                            tint = if (canUpdateByCode) updateTint else actionIconTint,
                                                            contentDescription = when {
                                                                canUpdateByCode -> stringResource(R.string.module_update)
                                                                equalByCode -> stringResource(R.string.module_reinstall)
                                                                else -> stringResource(R.string.install)
                                                            }
                                                        )
                                                        Text(
                                                            modifier = Modifier.padding(start = 4.dp, end = 2.dp),
                                                            text = when {
                                                                canUpdateByCode -> stringResource(R.string.module_update)
                                                                equalByCode -> stringResource(R.string.module_reinstall)
                                                                else -> stringResource(R.string.install)
                                                            },
                                                            color = if (canUpdateByCode) updateTint else actionIconTint,
                                                            fontWeight = FontWeight.Medium,
                                                            fontSize = 15.sp
                                                        )
                                                    }
                                                }
                                            }
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
            DownloadListener(context, onInstallModule)
        }
    }
}

@Composable
private fun ReadmePage(
    readmeText: String?,
    readmeLoaded: Boolean,
    innerPadding: PaddingValues,
    scrollBehavior: ScrollBehavior,
    hazeState: HazeState
) {
    LazyColumn(
        modifier = Modifier
            .height(getWindowSize().height.dp)
            .scrollEndHaptic()
            .overScrollVertical()
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .hazeSource(state = hazeState),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
            bottom = innerPadding.calculateBottomPadding(),
        ),
        overscrollEffect = null,
    ) {
        item {
            AnimatedVisibility(
                visible = readmeLoaded && readmeText != null,
                enter = expandVertically() + fadeIn(),
                exit = shrinkVertically() + fadeOut()
            ) {
                Column {
                    Spacer(Modifier.height(6.dp))
                    Card(
                        modifier = Modifier
                            .padding(horizontal = 12.dp),
                        insideMargin = PaddingValues(16.dp)
                    ) {
                        Column {
                            MarkdownContent(content = readmeText!!)
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
    LazyColumn(
        modifier = Modifier
            .height(getWindowSize().height.dp)
            .scrollEndHaptic()
            .overScrollVertical()
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .hazeSource(state = hazeState),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
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
                        .padding(bottom = 12.dp),
                    insideMargin = PaddingValues(16.dp)
                ) {
                    Column {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
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
                                modifier = Modifier.align(Alignment.Top)
                            )
                        }
                        AnimatedVisibility(
                            visible = rel.assets.isNotEmpty(),
                            enter = fadeIn() + expandVertically(),
                            exit = fadeOut() + shrinkVertically()
                        ) {
                            Column {
                                AnimatedVisibility(
                                    visible = rel.descriptionHTML.isNotBlank(),
                                    enter = fadeIn() + expandVertically(),
                                    exit = fadeOut() + shrinkVertically()
                                ) {
                                    Column {
                                        HorizontalDivider(
                                            modifier = Modifier.padding(vertical = 4.dp),
                                            thickness = 0.5.dp,
                                            color = colorScheme.outline.copy(alpha = 0.5f)
                                        )
                                        GithubMarkdownContent(content = rel.descriptionHTML)
                                    }
                                }
                                HorizontalDivider(
                                    modifier = Modifier.padding(vertical = 4.dp),
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
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        Column(modifier = Modifier.weight(1f)) {
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
                                                        imageVector = MiuixIcons.Useful.Save,
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
            }
        }
        item { Spacer(Modifier.height(12.dp)) }
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
    homepageUrl: String,
    sourceUrl: String,
) {
    LazyColumn(
        modifier = Modifier
            .height(getWindowSize().height.dp)
            .scrollEndHaptic()
            .overScrollVertical()
            .nestedScroll(scrollBehavior.nestedScrollConnection)
            .hazeSource(state = hazeState),
        contentPadding = PaddingValues(
            top = innerPadding.calculateTopPadding(),
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
                                val clickable = author.link.isNotBlank()
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
                                        imageVector = Icons.Rounded.Link,
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
        if (sourceUrl.isNotBlank() || homepageUrl.isNotBlank()) {
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
                        val clickable = sourceUrl.isNotBlank() || homepageUrl.isNotBlank()
                        val target = sourceUrl.ifBlank { homepageUrl }
                        val tint = if (clickable) actionIconTint else actionIconTint.copy(alpha = 0.35f)
                        IconButton(
                            backgroundColor = secondaryContainer,
                            minHeight = 35.dp,
                            minWidth = 35.dp,
                            enabled = clickable,
                            onClick = {
                                if (target.isNotBlank()) {
                                    uriHandler.openUri(target)
                                }
                            },
                        ) {
                            Icon(
                                modifier = Modifier.size(20.dp),
                                imageVector = Icons.Rounded.Link,
                                tint = tint,
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
@Destination<RootGraph>
fun ModuleRepoDetailScreen(
    navigator: DestinationsNavigator,
    module: RepoModuleArg
) {
    val context = LocalContext.current
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

    var readmeText by remember(module.moduleId) { mutableStateOf<String?>(null) }
    var readmeLoaded by remember(module.moduleId) { mutableStateOf(false) }
    var detailReleases by remember(module.moduleId) { mutableStateOf<List<ReleaseArg>>(emptyList()) }
    var homepageUrl by remember(module.moduleId) { mutableStateOf("") }
    var sourceUrl by remember(module.moduleId) { mutableStateOf("") }


    val scrollBehavior = MiuixScrollBehavior()

    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = colorScheme.surface,
        tint = HazeTint(colorScheme.surface.copy(0.8f))
    )

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
                        Icon(
                            imageVector = MiuixIcons.Useful.Back,
                            contentDescription = null,
                            tint = colorScheme.onSurface
                        )
                    }
                },
                actions = {
                    if (homepageUrl.isNotBlank()) {
                        IconButton(
                            modifier = Modifier.padding(end = 16.dp),
                            onClick = { uriHandler.openUri(homepageUrl) }
                        ) {
                            Icon(
                                imageVector = MiuixIcons.Useful.NavigatorSwitch,
                                contentDescription = null,
                                tint = colorScheme.onBackground
                            )
                        }
                    }
                }
            )
        }
    ) { innerPadding ->
        LaunchedEffect(module.moduleId) {
            if (module.moduleId.isNotEmpty()) {
                withContext(Dispatchers.IO) {
                    runCatching {
                        val detail = fetchModuleDetail(module.moduleId)
                        if (detail != null) {
                            readmeText = detail.readme
                            homepageUrl = (detail.homepageUrl ?: detail.url ?: "")
                            sourceUrl = (detail.sourceUrl ?: "")
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
        val density = LocalDensity.current
        var tabRowHeight by remember { mutableStateOf(0.dp) }
        var collapsedFraction by remember { mutableFloatStateOf(scrollBehavior.state.collapsedFraction) }
        LaunchedEffect(scrollBehavior.state.collapsedFraction) {
            snapshotFlow { scrollBehavior.state.collapsedFraction }.collectLatest { collapsedFraction = it }
        }
        val dynamicTopPadding = 12.dp * (1f - collapsedFraction)
        Box(
            modifier = Modifier.fillMaxSize()
        ) {
            HorizontalPager(
                state = pagerState,
                modifier = Modifier.fillMaxSize()
            ) { page ->
                val innerPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding() + tabRowHeight + dynamicTopPadding + 6.dp,
                    bottom = innerPadding.calculateBottomPadding()
                )
                when (page) {
                    0 -> ReadmePage(
                        readmeText = readmeText,
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
                        homepageUrl = homepageUrl,
                        sourceUrl = sourceUrl,
                    )
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
                    .padding(top = innerPadding.calculateTopPadding() + dynamicTopPadding, bottom = 6.dp)
                    .padding(horizontal = 12.dp)
                    .onSizeChanged { size ->
                        tabRowHeight = with(density) { size.height.toDp() }
                    }
            ) {
                TabRow(
                    tabs = tabs,
                    selectedTabIndex = pagerState.currentPage,
                    onTabSelected = { index -> scope.launch { pagerState.animateScrollToPage(index) } },
                    colors = TabRowDefaults.tabRowColors(
                        backgroundColor = Color.Transparent,
                    ),
                    height = 40.dp,
                )
            }
        }
        DownloadListener(context, onInstallModule)
    }
}
