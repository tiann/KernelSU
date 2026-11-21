package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import ui.feice.mochi.basic.MochiSkeleton
import ui.feice.mochi.shape.MochiRoundedRectangle
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.width
import androidx.compose.ui.Alignment
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.RepoRelease
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.screen.modulerepo.AuthorArg
import me.weishu.kernelsu.ui.screen.modulerepo.ModuleRepoActions
import me.weishu.kernelsu.ui.screen.modulerepo.ModuleRepoDetailActions
import me.weishu.kernelsu.ui.screen.modulerepo.ModuleRepoDetailUiState
import me.weishu.kernelsu.ui.screen.modulerepo.ModuleRepoUiState
import me.weishu.kernelsu.ui.screen.modulerepo.RepoInstallState
import me.weishu.kernelsu.ui.screen.modulerepo.RepoModuleIcon
import me.weishu.kernelsu.ui.screen.modulerepo.RepoSort
import me.weishu.kernelsu.ui.screen.modulerepo.formatFileSize
import me.weishu.kernelsu.ui.screen.modulerepo.formatStarCount
import me.weishu.kernelsu.ui.screen.modulerepo.rememberReleaseDownload
import me.weishu.kernelsu.ui.screen.modulerepo.repoInstallState
import me.weishu.kernelsu.ui.screen.modulerepo.repoInstallStateOf
import me.weishu.kernelsu.ui.screen.modulerepo.shortHash
import me.weishu.kernelsu.ui.screen.modulerepo.startReleaseDownload
import me.weishu.kernelsu.ui.screen.modulerepo.starsUrl
import me.weishu.kernelsu.ui.screen.scriptrun.lastRunText
import me.weishu.kernelsu.ui.util.isDownloadAvailable
import ui.feice.mochi.basic.MochiActivityIndicator
import ui.feice.mochi.basic.MochiBarIconButton
import ui.feice.mochi.basic.MochiBarIconDropdown
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.MochiEmptyState
import ui.feice.mochi.basic.MochiItem
import ui.feice.mochi.basic.MochiDownloadButton
import ui.feice.mochi.basic.MochiDownloadState
import ui.feice.mochi.basic.MochiSearchBar
import ui.feice.mochi.basic.MochiSegmentedControl
import ui.feice.mochi.basic.Text
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListDefaults
import ui.feice.mochi.list.MochiListLeadingStyle
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.list.mochiListSection
import ui.feice.mochi.list.rows
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold
import ui.feice.mochi.theme.MochiTheme

@Composable
private fun RepoBadges(metamodule: Boolean, isScript: Boolean, installState: RepoInstallState) {
    val cs = MochiTheme.colorScheme
    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        if (metamodule) MochiListStatusChip("META", containerColor = cs.tertiaryContainer, contentColor = cs.onTertiaryContainer)
        if (isScript) {
            MochiListStatusChip(stringResource(R.string.module_repo_script_badge), containerColor = cs.secondaryContainer, contentColor = cs.onSecondaryContainer)
        }
        when (installState) {
            RepoInstallState.UPDATABLE -> MochiListStatusChip(stringResource(R.string.module_repo_update_available), containerColor = cs.error, contentColor = cs.onError)
            RepoInstallState.INSTALLED -> MochiListStatusChip(stringResource(R.string.module_repo_installed))
            RepoInstallState.NONE -> Unit
        }
    }
}

@Composable
fun ModuleRepoScreenMochi(state: ModuleRepoUiState, actions: ModuleRepoActions) {
    val refreshing = rememberUpdatedState(state.isRefreshing)
    val query = state.searchStatus.searchText
    val searching = query.isNotBlank()
    val modules = if (searching) state.searchResults else state.modules
    var menu by remember { mutableStateOf(false) }
    val sorts = listOf(
        RepoSort.UPDATED to R.string.module_repos_sort_updated,
        RepoSort.NAME to R.string.module_repos_sort_name,
        RepoSort.STARS to R.string.module_repos_sort_stars,
    )
    val sortLabels = sorts.map { stringResource(it.second) }
    val sortLabel = stringResource(R.string.menu_sort)
    val authorLabel = stringResource(R.string.module_author)

    MochiLazyLargeTitleScaffold(
        title = stringResource(R.string.module_repos),
        navigationIcon = { MochiBackButton() },
        actions = {
            MochiBarIconDropdown(
                expanded = menu,
                onExpandedChange = { menu = it },
                items = sorts.mapIndexed { i, (sort, _) -> MochiItem(sortLabels[i], icon = checkIcon(state.sortOrder == sort)) }.toImmutableList(),
                onItemClick = { actions.onSetSortOrder(sorts[it].first) },
                modifier = Modifier.semantics { contentDescription = sortLabel },
                navId = "repo.sort",
                contentKey = "filter",
            ) { MochiIonicon("filter") }
        },
        searchBar = {
            MochiSearchBar(
                query = query,
                onQueryChange = actions.onSearchTextChange,
                placeholder = stringResource(R.string.mochi_search_modules),
                onCancel = actions.onClearSearch,
            )
        },
        onRefresh = { awaitRefresh(actions.onRefresh, refreshing) },
    ) {
        val empty = state.emptyStateMessage
        when {
            empty != null && !searching -> item(key = "empty") {
                MochiEmptyState(
                    stringResource(empty),
                    icon = { MochiIonicon(if (state.offline) "cloud-offline" else "storefront", size = 44.dp) },
                    action = {
                        MochiButton(onClick = actions.onRefresh, style = MochiButtonStyle.ProminentGlass) {
                            Text(stringResource(R.string.network_retry))
                        }
                    },
                )
            }

            modules.isEmpty() && state.isRefreshing -> item(key = "loading") {
                RepoSkeleton()
            }

            modules.isEmpty() -> item(key = "none") {
                MochiEmptyState(stringResource(R.string.mochi_no_results), icon = { MochiIonicon("search", size = 44.dp) })
            }

            else -> {
                if (state.stale) {
                    item(key = "stale") {
                        MochiListCard {
                            MochiListRow(
                                stringResource(R.string.module_repo_using_cache),
                                leading = { MochiIonicon("time", tint = MochiTheme.colorScheme.onSurfaceVariant) },
                                leadingStyle = MochiListLeadingStyle.Symbol,
                            )
                        }
                    }
                }
                mochiListSection(key = if (searching) "results" else "modules") {
                    rows(modules, key = { it.moduleId }) { module ->
                        val installState = repoInstallStateOf(module, state.installedVersions)
                        val hasBadges = module.metamodule || module.isScript || installState != RepoInstallState.NONE
                        MochiListRow(
                            module.moduleName,
                            subtitle = buildString {
                                append("$authorLabel: ${module.authors}")
                                if (module.summary.isNotBlank()) append('\n').append(module.summary)
                            },
                            value = module.stars?.let { "★ ${formatStarCount(it)}" },
                            leading = if (module.iconUrl != null) {
                                { RepoModuleIcon(module.iconUrl, size = 28.dp) }
                            } else {
                                { KsuTile("extension-puzzle", KsuTileColors.Indigo) }
                            },
                            status = if (hasBadges) {
                                { RepoBadges(module.metamodule, module.isScript, installState) }
                            } else {
                                null
                            },
                            chevron = true,
                            onClick = { actions.onOpenRepoDetail(module) },
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun ModuleRepoDetailScreenMochi(state: ModuleRepoDetailUiState, actions: ModuleRepoDetailActions) {
    val context = LocalContext.current
    val module = state.module
    val detail = state.detail
    var tab by rememberSaveable { mutableStateOf(0) }
    var pending by remember { mutableStateOf<(() -> Unit)?>(null) }
    val confirm = rememberConfirmDialog(onConfirm = { pending?.invoke() })
    val confirmTitle = stringResource(if (state.isScript) R.string.script_run_title else R.string.module_install)
    val webLabel = stringResource(R.string.module_repos_source_code)
    val tabs = persistentListOf(stringResource(R.string.tab_readme), stringResource(R.string.tab_releases), stringResource(R.string.tab_info))
    val installState = repoInstallState(state.installedVersionCode, detail?.module?.latestVersionCode ?: module.latestVersionCode)

    MochiLazyCompactTitleScaffold(
        title = detail?.module?.moduleName ?: module.moduleName,
        subtitle = module.authors,
        navigationIcon = { MochiBackButton() },
        actions = {
            if (state.webUrl.isNotEmpty()) {
                MochiBarIconButton(
                    onClick = actions.onOpenWebUrl,
                    modifier = Modifier.semantics { contentDescription = webLabel },
                    navId = "repo.web",
                    contentKey = "open",
                ) { MochiIonicon("open") }
            }
        },
    ) {
        item(key = "header") {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                MochiListCard {
                    MochiListRow(
                        "${detail?.module?.latestVersion ?: module.latestVersion} (${detail?.module?.latestVersionCode ?: module.latestVersionCode})",
                        subtitle = detail?.module?.summary?.takeIf { it.isNotBlank() } ?: module.latestReleaseTime,
                        leading = detail?.module?.iconUrl?.let { url -> { RepoModuleIcon(url, size = 28.dp) } }
                            ?: { KsuTile("extension-puzzle", KsuTileColors.Indigo) },
                        status = { RepoBadges(module.metamodule, state.isScript, if (state.isScript) RepoInstallState.NONE else installState) },
                    )
                }
                MochiSegmentedControl(selectedIndex = tab, onSelect = { tab = it }, items = tabs, modifier = Modifier.fillMaxWidth())
            }
        }
        when {
            state.notInIndex -> item(key = "notindex") {
                MochiEmptyState(stringResource(R.string.module_repo_not_in_index), icon = { MochiIonicon("eye-off", size = 44.dp) })
            }

            state.loadFailed && detail == null -> item(key = "failed") {
                MochiEmptyState(
                    stringResource(R.string.module_repo_load_failed),
                    icon = { MochiIonicon("cloud-offline", size = 44.dp) },
                    action = {
                        MochiButton(onClick = actions.onRetry, style = MochiButtonStyle.ProminentGlass) { Text(stringResource(R.string.network_retry)) }
                    },
                )
            }

            detail == null -> item(key = "loading") {
                RepoSkeleton()
            }

            tab == 0 -> item(key = "readme") {
                val readme = detail.readmeHtml
                MochiListCard {
                    Column(Modifier.fillMaxWidth().padding(horizontal = MochiListDefaults.RowPadding, vertical = 14.dp)) {
                        when {
                            readme != null -> MochiRichText(readme, isMarkdown = false)
                            detail.readmeFailed -> MochiFootnote(stringResource(R.string.module_repo_readme_failed))
                            else -> MochiFootnote(stringResource(R.string.module_repo_no_readme))
                        }
                    }
                }
            }

            tab == 1 -> {
                if (detail.releases.isEmpty()) {
                    item(key = "noreleases") { MochiEmptyState(stringResource(R.string.mochi_no_results), icon = { MochiIonicon("pricetags", size = 44.dp) }) }
                }
                detail.releases.forEach { release ->
                    item(key = "release_${release.tag}") {
                        ReleaseCard(
                            release = release,
                            canInstall = state.canInstall,
                            isScript = state.isScript,
                            onOpenUrl = actions.onOpenUrl,
                            onInstall = actions.onInstallModule,
                            onConfirmDownload = { start ->
                                pending = start
                                confirm.showConfirm(title = confirmTitle, content = context.getString(R.string.module_start_downloading, release.zipName))
                            },
                        )
                    }
                }
            }

            else -> item(key = "info") { InfoSections(state, actions) }
        }
    }
}

@Composable
private fun ReleaseCard(
    release: RepoRelease,
    canInstall: Boolean,
    isScript: Boolean,
    onOpenUrl: (String) -> Unit,
    onInstall: (android.net.Uri) -> Unit,
    onConfirmDownload: (() -> Unit) -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val handle = rememberReleaseDownload(release)
    val downloaded = handle.downloadedUri
    MochiListCard {
        MochiListRow(
            release.name,
            subtitle = if (release.name == release.tag) release.version else "${release.tag} · ${release.version}",
            value = release.publishedAt.take(10),
        )
        release.changelogHtml?.let { html ->
            Column(Modifier.fillMaxWidth().padding(horizontal = MochiListDefaults.RowPadding, vertical = 12.dp)) {
                MochiRichText(html, isMarkdown = false)
            }
        }
        MochiListRow(
            release.zipName,
            subtitle = buildString {
                append(formatFileSize(release.zipSize))
                append("\nSHA-256: ").append(shortHash(release.sha256))
                if (release.immutable) append('\n').append(context.getString(R.string.module_repo_immutable_release))
            },
            trailing = if (canInstall) {
                {
                    // download -> progress -> install
                    MochiDownloadButton(
                        state = when {
                            downloaded != null -> MochiDownloadState.Done
                            handle.isDownloading && handle.progress <= 0 -> MochiDownloadState.Waiting
                            handle.isDownloading -> MochiDownloadState.Loading(handle.progress / 100f)
                            else -> MochiDownloadState.Idle
                        },
                        label = stringResource(R.string.download),
                        doneLabel = stringResource(if (isScript) R.string.script_run else R.string.install),
                        onClick = {
                            when {
                                downloaded != null -> scope.launch {
                                    if (isDownloadAvailable(downloaded)) onInstall(downloaded) else handle.setDownloaded(null)
                                }

                                handle.isDownloading -> Unit

                                else -> onConfirmDownload { scope.startReleaseDownload(context, release, handle) }
                            }
                        },
                        modifier = Modifier.padding(start = 8.dp),
                    )
                }
            } else {
                null
            },
        )
        MochiListRow(
            stringResource(R.string.module_repo_release_page),
            subtitle = release.tagCommit.take(12),
            chevron = true,
            onClick = { onOpenUrl(release.releaseUrl) },
        )
        release.attestationUrl?.let { url ->
            MochiListRow(stringResource(R.string.module_repo_attestation), chevron = true, onClick = { onOpenUrl(url) })
        }
    }
}

@Composable
private fun InfoSections(state: ModuleRepoDetailUiState, actions: ModuleRepoDetailActions) {
    val detail = state.detail ?: return
    val authors = detail.module.authorList.map { AuthorArg(it.name, it.link) }.ifEmpty { state.module.authorsList }
    val context = LocalContext.current
    Column(verticalArrangement = Arrangement.spacedBy(MochiListDefaults.SectionSpacing)) {
        val notices = buildList {
            if (state.isScript) add(lastRunText(context, state.lastRun))
            if (detail.healthStale) add(stringResource(R.string.module_repo_health_stale))
            if (detail.fromCache) add(stringResource(R.string.module_repo_using_cache))
        }
        if (notices.isNotEmpty()) {
            MochiListSection {
                notices.forEach { MochiListRow(it, leading = { MochiIonicon("information-circle") }, leadingStyle = MochiListLeadingStyle.Symbol) }
            }
        }
        MochiListSection(header = stringResource(R.string.module_author)) {
            authors.forEach { author ->
                MochiListRow(
                    author.name,
                    chevron = author.link.isNotEmpty(),
                    onClick = if (author.link.isNotEmpty()) {
                        { actions.onOpenUrl(author.link) }
                    } else {
                        null
                    },
                )
            }
        }
        MochiListSection(header = stringResource(R.string.module_repos_source_code)) {
            MochiListRow(
                detail.sourceFullName,
                subtitle = stringResource(R.string.module_repo_source_commit, detail.sourceCommit.take(12)),
                chevron = true,
                onClick = { actions.onOpenUrl(detail.sourceUrl) },
            )
        }
        MochiListSection(header = stringResource(R.string.tab_info), footer = stringResource(R.string.module_repo_integrity_desc)) {
            MochiListRow(stringResource(R.string.module_version), value = "${detail.module.latestVersion} (${detail.module.latestVersionCode})")
            detail.module.tags.takeIf { it.isNotEmpty() }?.let { MochiListRow(stringResource(R.string.module_repo_tags), subtitle = it.joinToString(", ")) }
            detail.module.stars?.let { stars ->
                val url = starsUrl(detail.module.starsRepository)
                MochiListRow(
                    stringResource(R.string.module_repo_stars),
                    value = formatStarCount(stars),
                    chevron = url != null,
                    onClick = url?.let { { actions.onOpenUrl(it) } },
                )
            }
            detail.license?.let { MochiListRow(stringResource(R.string.module_repo_license), value = it) }
            detail.homepage?.let { url -> MochiListRow(stringResource(R.string.module_repo_homepage), subtitle = url, chevron = true, onClick = { actions.onOpenUrl(url) }) }
            detail.support?.let { url -> MochiListRow(stringResource(R.string.module_repo_support), subtitle = url, chevron = true, onClick = { actions.onOpenUrl(url) }) }
            MochiListRow(stringResource(R.string.module_repo_integrity), leading = { MochiIonicon("shield-checkmark", tint = MochiTheme.colorScheme.primary) }, leadingStyle = MochiListLeadingStyle.Symbol)
        }
    }
}

/** Skeleton rows while loading */
@Composable
private fun RepoSkeleton(rows: Int = 5) {
    val fill = MochiTheme.colorScheme.onSurface.copy(alpha = 0.08f)
    MochiListCard {
        repeat(rows) { i ->
            Row(
                Modifier
                    .fillMaxWidth()
                    .padding(horizontal = MochiListDefaults.RowPadding, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                MochiSkeleton(true, shape = MochiRoundedRectangle(7.dp), color = fill) { Box(Modifier.size(28.dp)) }
                Spacer(Modifier.width(MochiListDefaults.LeadingGap))
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    MochiSkeleton(true, shape = MochiRoundedRectangle(4.dp), color = fill) {
                        Box(Modifier.fillMaxWidth(if (i % 2 == 0) 0.55f else 0.4f).height(14.dp))
                    }
                    MochiSkeleton(true, shape = MochiRoundedRectangle(4.dp), color = fill) {
                        Box(Modifier.fillMaxWidth(if (i % 2 == 0) 0.8f else 0.7f).height(11.dp))
                    }
                }
            }
        }
    }
}
