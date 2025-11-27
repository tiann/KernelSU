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
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Link
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.MarkdownContent
import me.weishu.kernelsu.ui.component.SearchBox
import me.weishu.kernelsu.ui.component.SearchPager
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.DownloadListener
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.viewmodel.ModuleRepoViewModel
import okhttp3.Request
import org.json.JSONObject
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.SmallTitle
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.icon.icons.useful.NavigatorSwitch
import top.yukonga.miuix.kmp.icon.icons.useful.Save
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Parcelize
data class ReleaseAssetArg(
    val name: String,
    val downloadUrl: String,
    val size: Long
) : Parcelable

@Parcelize
data class ReleaseArg(
    val tagName: String,
    val name: String,
    val publishedAt: String,
    val assets: List<ReleaseAssetArg>
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
    val homepageUrl: String,
    val sourceUrl: String,
    val latestRelease: String,
    val latestReleaseTime: String,
    val releases: List<ReleaseArg>
) : Parcelable

@SuppressLint("StringFormatInvalid")
@Composable
@Destination<RootGraph>
fun ModuleRepoScreen(
    navigator: DestinationsNavigator
) {
    val viewModel = viewModel<ModuleRepoViewModel>()
    val searchStatus by viewModel.searchStatus
    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val isDark = isInDarkTheme(prefs.getInt("color_mode", 0))
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isDark) 0.7f else 0.9f)
    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        if (viewModel.modules.value.isEmpty()) {
            viewModel.refresh()
        }
    }

    val scrollBehavior = MiuixScrollBehavior()
    val dynamicTopPadding by remember {
        derivedStateOf { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }
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

    val confirmTitle = stringResource(R.string.module)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })

    Scaffold(
        topBar = {
            searchStatus.TopAppBarAnim(hazeState = hazeState, hazeStyle = hazeStyle) {
                TopAppBar(
                    color = Color.Transparent,
                    title = stringResource(R.string.module_repo),
                    scrollBehavior = scrollBehavior,
                    navigationIcon = {
                        IconButton(
                            modifier = Modifier.padding(start = 16.dp),
                            onClick = { navigator.popBackStack() }
                        ) {
                            Icon(
                                imageVector = MiuixIcons.Useful.Back,
                                contentDescription = null,
                                tint = colorScheme.onSurface
                            )
                        }
                    }
                )
            }
        },
        popupHost = {
            searchStatus.SearchPager(defaultResult = {}) {
                item {
                    Spacer(Modifier.height(6.dp))
                }
                items(viewModel.searchResults.value, key = { it.moduleId }) { module ->
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
                                homepageUrl = module.homepageUrl,
                                sourceUrl = module.sourceUrl,
                                latestRelease = module.latestRelease,
                                latestReleaseTime = module.latestReleaseTime,
                                releases = module.releases.map { r ->
                                    ReleaseArg(
                                        tagName = r.tagName,
                                        name = r.name,
                                        publishedAt = r.publishedAt,
                                        assets = r.assets.map { a ->
                                            ReleaseAssetArg(name = a.name, downloadUrl = a.downloadUrl, size = a.size)
                                        }
                                    )
                                }
                            )
                            navigator.navigate(ModuleRepoDetailScreenDestination(args)) { launchSingleTop = true }
                        }
                    ) {
                        Column {
                            if (module.moduleName.isNotBlank()) {
                                Text(
                                    text = module.moduleName,
                                    fontSize = 17.sp,
                                    fontWeight = FontWeight(550),
                                    color = colorScheme.onSurface
                                )
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

        if (isLoading) {
            Box(
                modifier = Modifier
                    .fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                InfiniteProgressIndicator()
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
                        delay(350)
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
                            items = viewModel.modules.value,
                            key = { it.moduleId },
                            contentType = { "module" }
                        ) { module ->
                            val latestTag = module.latestRelease
                            val latestRel = remember(module.releases, latestTag) {
                                module.releases.find { it.tagName == latestTag } ?: module.releases.firstOrNull()
                            }
                            val latestAsset = remember(latestRel) { latestRel?.assets?.firstOrNull() }

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
                                        homepageUrl = module.homepageUrl,
                                        sourceUrl = module.sourceUrl,
                                        latestRelease = module.latestRelease,
                                        latestReleaseTime = module.latestReleaseTime,
                                        releases = module.releases.map { r ->
                                            ReleaseArg(
                                                tagName = r.tagName,
                                                name = r.name,
                                                publishedAt = r.publishedAt,
                                                assets = r.assets.map { a ->
                                                    ReleaseAssetArg(name = a.name, downloadUrl = a.downloadUrl, size = a.size)
                                                }
                                            )
                                        }
                                    )
                                    navigator.navigate(ModuleRepoDetailScreenDestination(args)) {
                                        launchSingleTop = true
                                    }
                                }
                            ) {
                                Column {
                                    if (module.moduleName.isNotBlank()) {
                                        Text(
                                            text = module.moduleName,
                                            fontSize = 17.sp,
                                            fontWeight = FontWeight(550),
                                            color = colorScheme.onSurface
                                        )
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
                                            val downloadingText = stringResource(R.string.module_downloading)
                                            IconButton(
                                                backgroundColor = colorScheme.secondaryContainer.copy(alpha = 0.8f),
                                                minHeight = 35.dp,
                                                minWidth = 35.dp,
                                                onClick = {
                                                    pendingDownload = {
                                                        scope.launch(Dispatchers.IO) {
                                                            download(
                                                                context,
                                                                latestAsset.downloadUrl,
                                                                fileName,
                                                                downloadingText.format(module.moduleId)
                                                            )
                                                        }
                                                    }
                                                    val confirmContent =
                                                        context.getString(R.string.module_install_prompt_with_name, fileName)
                                                    confirmDialog.showConfirm(
                                                        title = confirmTitle,
                                                        content = confirmContent
                                                    )
                                                },
                                            ) {
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
                                }
                            }
                        }
                        item { Spacer(Modifier.height(12.dp)) }
                    }
                }
            }
            DownloadListener(context, onInstallModule)
        }
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
    val confirmTitle = stringResource(R.string.module)
    var pendingDownload by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirmDialog = rememberConfirmDialog(onConfirm = { pendingDownload?.invoke() })

    var readmeText by remember(module.moduleId) { mutableStateOf<String?>(null) }
    var readmeLoaded by remember(module.moduleId) { mutableStateOf(false) }

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
                title = module.moduleId,
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
                    if (module.homepageUrl.isNotBlank()) {
                        IconButton(
                            modifier = Modifier.padding(end = 16.dp),
                            onClick = { uriHandler.openUri(module.homepageUrl) }
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
                        val url = "https://modules.kernelsu.org/module/${module.moduleId}.json"
                        ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
                            if (!resp.isSuccessful) return@use
                            val body = resp.body?.string() ?: return@use
                            val obj = JSONObject(body)
                            val readme = obj.optString("readme", "")
                            readmeText = readme.ifBlank { null }
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
        ) {
            item {
                AnimatedVisibility(
                    visible = readmeLoaded && readmeText != null,
                    enter = expandVertically() + fadeIn(),
                    exit = shrinkVertically() + fadeOut()
                ) {
                    Column {
                        SmallTitle(text = "README")
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
            if (module.authorsList.isNotEmpty()) {
                item {
                    SmallTitle(
                        text = "AUTHORS",
                        modifier = Modifier.padding(top = 10.dp)
                    )
                }
                item {
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
            if (module.releases.isNotEmpty()) {
                item {
                    SmallTitle(
                        text = "RELEASES",
                        modifier = Modifier.padding(top = 10.dp)
                    )
                }
                items(
                    items = module.releases,
                    key = { it.tagName },
                    contentType = { "release" }
                ) { rel ->
                    val title = remember(rel.name, rel.tagName) { rel.name.ifBlank { rel.tagName } }

                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 12.dp)
                            .padding(bottom = 8.dp),
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
                                )
                            }

                            AnimatedVisibility(
                                visible = rel.assets.isNotEmpty(),
                                enter = fadeIn() + expandVertically(),
                                exit = fadeOut() + shrinkVertically()
                            ) {
                                Column {
                                    HorizontalDivider(
                                        modifier = Modifier.padding(vertical = 8.dp),
                                        thickness = 0.5.dp,
                                        color = colorScheme.outline.copy(alpha = 0.5f)
                                    )

                                    rel.assets.forEachIndexed { index, asset ->
                                        val fileName = asset.name
                                        val downloadingText = stringResource(R.string.module_downloading)
                                        val sizeText = remember(asset.size) {
                                            val s = asset.size
                                            when {
                                                s >= 1024L * 1024L * 1024L -> String.format("%.1f GB", s / (1024f * 1024f * 1024f))
                                                s >= 1024L * 1024L -> String.format("%.1f MB", s / (1024f * 1024f))
                                                s >= 1024L -> String.format("%.0f KB", s / 1024f)
                                                else -> "$s B"
                                            }
                                        }
                                        val onClickDownload = remember(fileName, asset.downloadUrl) {
                                            {
                                                pendingDownload = {
                                                    scope.launch(Dispatchers.IO) {
                                                        download(
                                                            context,
                                                            asset.downloadUrl,
                                                            fileName,
                                                            downloadingText.format(module.moduleId)
                                                        )
                                                    }
                                                }
                                                val confirmContent = context.getString(R.string.module_install_prompt_with_name, fileName)
                                                confirmDialog.showConfirm(
                                                    title = confirmTitle,
                                                    content = confirmContent
                                                )
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
                                                    text = sizeText,
                                                    fontSize = 12.sp,
                                                    color = colorScheme.onSurfaceVariantSummary,
                                                    modifier = Modifier.padding(top = 2.dp)
                                                )
                                            }
                                            IconButton(
                                                backgroundColor = secondaryContainer,
                                                minHeight = 35.dp,
                                                minWidth = 35.dp,
                                                onClick = onClickDownload,
                                            ) {
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
}
