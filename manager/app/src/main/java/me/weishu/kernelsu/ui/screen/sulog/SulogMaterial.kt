package me.weishu.kernelsu.ui.screen.sulog

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.FilterList
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenuGroup
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.DropdownMenuPopup
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme.colorScheme
import androidx.compose.material3.MaterialTheme.typography
import androidx.compose.material3.MenuDefaults
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.ScrollToTopOnChange
import me.weishu.kernelsu.ui.component.material.ExpressiveScaffold
import me.weishu.kernelsu.ui.component.material.SearchAppBar
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedDropdownItem
import me.weishu.kernelsu.ui.component.material.SegmentedItem
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.TonalCard
import me.weishu.kernelsu.ui.component.material.TopBarBackButton
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.util.SulogEntry
import me.weishu.kernelsu.ui.util.SulogEventFilter

@Composable
fun SulogScreenMaterial(
    state: SulogScreenState,
    actions: SulogActions,
) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val pullToRefreshState = rememberPullToRefreshState()
    val listState = rememberLazyListState()
    val searchListState = rememberLazyListState()
    val haptic = LocalHapticFeedback.current
    val fileSelector = buildSulogFileSelector(state.files, state.selectedFilePath)
    var selectedEntry by remember { mutableStateOf<SulogEntry?>(null) }
    var showFilterMenu by remember { mutableStateOf(false) }
    var localSearchText by remember { mutableStateOf(state.searchText) }

    LaunchedEffect(state.searchText) {
        localSearchText = state.searchText
    }

    if (selectedEntry != null) {
        SulogDetailDialog(
            entry = selectedEntry!!,
            onDismiss = { selectedEntry = null },
        )
    }

    val snackbarHostState = remember { SnackbarHostState() }

    ExpressiveScaffold(
        topBar = {
            SearchAppBar(
                snackbarHostState = snackbarHostState,
                title = { Text(stringResource(R.string.settings_sulog)) },
                searchText = localSearchText,
                onSearchTextChange = {
                    localSearchText = it
                    actions.onSearchTextChange(it)
                },
                onClearClick = {
                    localSearchText = ""
                    actions.onSearchTextChange("")
                },
                navigationIcon = {
                    TopBarBackButton(onClick = actions.onBack)
                },
                actions = {
                    IconButton(onClick = actions.onCleanFile) {
                        Icon(
                            imageVector = Icons.Filled.DeleteSweep,
                            contentDescription = stringResource(R.string.sulog_clean_title),
                        )
                    }
                    IconButton(onClick = { showFilterMenu = true }) {
                        Icon(
                            imageVector = Icons.Filled.FilterList,
                            contentDescription = stringResource(R.string.sulog_filter_title),
                        )
                    }
                    DropdownMenuPopup(
                        expanded = showFilterMenu,
                        onDismissRequest = { showFilterMenu = false },
                    ) {
                        val filters = SulogEventFilter.entries
                        DropdownMenuGroup(shapes = MenuDefaults.groupShapes()) {
                            filters.forEachIndexed { index, filter ->
                                DropdownMenuItem(
                                    text = { Text(sulogFilterLabel(filter)) },
                                    checked = filter in state.selectedFilters,
                                    checkedLeadingIcon = {
                                        Icon(
                                            Icons.Filled.Check,
                                            modifier = Modifier.size(MenuDefaults.LeadingIconSize),
                                            contentDescription = null,
                                        )
                                    },
                                    onCheckedChange = {
                                        haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                        actions.onToggleFilter(filter)
                                    },
                                    shapes = MenuDefaults.itemShape(index = index, count = filters.size),
                                )
                            }
                        }
                    }
                },
                scrollBehavior = scrollBehavior,
                searchContent = { bottomPadding, _ ->
                    val latestVisibleEntries = rememberUpdatedState(state.visibleEntries)
                    ScrollToTopOnChange(
                        searchListState,
                        state.searchText,
                    ) { latestVisibleEntries.value }
                    LazyColumn(
                        state = searchListState,
                        modifier = Modifier
                            .fillMaxSize()
                            .nestedScroll(scrollBehavior.nestedScrollConnection),
                        contentPadding = PaddingValues(
                            start = 16.dp,
                            end = 16.dp,
                            bottom = 16.dp + bottomPadding,
                        ),
                    ) {
                        sulogEntriesSection(
                            entries = state.visibleEntries,
                            errorMessage = state.errorMessage,
                            onEntryClick = { selectedEntry = it },
                        )
                    }
                },
            )
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
    ) { innerPadding ->
        PullToRefreshBox(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding),
            isRefreshing = state.isLoading || state.isRefreshing,
            onRefresh = {
                haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                actions.onRefresh()
            },
            state = pullToRefreshState,
            indicator = {
                PullToRefreshDefaults.LoadingIndicator(
                    modifier = Modifier.align(Alignment.TopCenter),
                    isRefreshing = state.isLoading || state.isRefreshing,
                    state = pullToRefreshState,
                )
            },
        ) {
            val latestEntries = rememberUpdatedState(state.visibleEntries)
            ScrollToTopOnChange(
                listState,
                state.selectedFilters,
                state.selectedFilePath,
            ) { latestEntries.value }
            LazyColumn(
                state = listState,
                modifier = Modifier
                    .fillMaxSize()
                    .nestedScroll(scrollBehavior.nestedScrollConnection),
                contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 8.dp),
            ) {
                item {
                    SulogStatusSection(state, actions)
                }

                item {
                    Box(modifier = Modifier.padding(bottom = 16.dp)) {
                        SegmentedColumn(
                            content = listOf {
                                SegmentedDropdownItem(
                                    title = stringResource(R.string.sulog_log_files),
                                    items = fileSelector.items,
                                    enabled = fileSelector.items.isNotEmpty(),
                                    selectedIndex = fileSelector.selectedIndex,
                                    onItemSelected = { index ->
                                        state.files.getOrNull(index)?.let { file ->
                                            actions.onSelectFile(file.path)
                                        }
                                    }
                                )
                            },
                        )
                    }
                }

                sulogEntriesSection(
                    entries = state.visibleEntries,
                    errorMessage = state.errorMessage,
                    onEntryClick = { selectedEntry = it },
                )

                item {
                    Spacer(
                        Modifier.height(
                            WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                    WindowInsets.captionBar.asPaddingValues().calculateBottomPadding() +
                                    16.dp
                        )
                    )
                }
            }
        }
    }
}

private fun LazyListScope.sulogEntriesSection(
    entries: List<SulogEntry>,
    errorMessage: String?,
    onEntryClick: (SulogEntry) -> Unit,
) {
    when {
        errorMessage != null -> item {
            SulogMessageCard(
                modifier = Modifier.fillParentMaxSize(),
                title = stringResource(R.string.sulog_failed_to_load),
                summary = errorMessage,
            )
        }

        else -> {
            itemsIndexed(entries, key = { index, entry -> "$index-${entry.key}" }) { index, entry ->
                SegmentedItem(index = index, count = entries.size) {
                    SegmentedListItem(
                        modifier = if (index < entries.lastIndex) {
                            Modifier.padding(bottom = 2.dp)
                        } else {
                            Modifier
                        },
                        onClick = { onEntryClick(entry) },
                        headlineContent = { Text(sulogEntryTitle(entry)) },
                        supportingContent = {
                            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                                sulogEntryDescription(entry)?.let {
                                    Text(
                                        it,
                                        style = typography.bodySmall,
                                        maxLines = 2,
                                        overflow = TextOverflow.Ellipsis
                                    )
                                }
                                entry.timestampText?.let {
                                    Text(
                                        it,
                                        style = typography.labelMediumEmphasized,
                                        color = colorScheme.onSurfaceVariant,
                                    )
                                }
                                Row(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                                    val colors = listOf(
                                        colorScheme.primary to colorScheme.onPrimary,
                                        colorScheme.secondary to colorScheme.onSecondary,
                                        colorScheme.tertiary to colorScheme.onTertiary,
                                    )
                                    sulogEntrySummaryTags(entry).forEachIndexed { index, tag ->
                                        val (bg, fg) = colors.getOrElse(index) { colors.last() }
                                        StatusTag(label = tag, backgroundColor = bg, contentColor = fg)
                                    }
                                }
                            }
                        },
                        trailingContent = {
                            sulogEntryStatus(entry)?.let { Text(it) }
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun SulogStatusSection(
    state: SulogScreenState,
    actions: SulogActions,
) {
    when (state.sulogStatus) {
        "unsupported" -> {
            WarningCard(text = stringResource(R.string.sulog_unsupported_title))
        }

        "managed" -> {
            WarningCard(text = stringResource(R.string.feature_status_managed_summary))
        }

        "supported" if !state.isSulogEnabled -> {
            WarningCard(
                text = stringResource(R.string.sulog_disabled_title),
                action = {
                    Button(
                        onClick = actions.onEnableSulog,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = colorScheme.error,
                            contentColor = colorScheme.onError,
                        ),
                    ) {
                        Text(stringResource(R.string.sulog_enable_action))
                    }
                },
            )
        }

        else -> Unit
    }
}

@Composable
private fun SulogMessageCard(
    modifier: Modifier,
    title: String,
    summary: String? = null,
) {
    Box(
        modifier = modifier,
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(title, color = colorScheme.onSurfaceVariant)
            if (summary != null) {
                Text(
                    summary,
                    color = colorScheme.onSurfaceVariant,
                    fontSize = typography.bodySmall.fontSize,
                    maxLines = 3,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

@Composable
private fun WarningCard(
    text: String,
    action: (@Composable () -> Unit)? = null,
) {
    TonalCard(
        modifier = Modifier.padding(bottom = 16.dp),
        containerColor = colorScheme.errorContainer
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp, horizontal = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = text,
                style = typography.bodyLarge,
                modifier = Modifier.weight(1f),
            )
            action?.invoke()
        }
    }
}

@Composable
private fun SulogDetailDialog(
    entry: SulogEntry,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(sulogEntryTitle(entry)) },
        text = {
            Column(
                modifier = Modifier.verticalScroll(rememberScrollState())
            ) {
                SelectionContainer {
                    Text(
                        text = sulogEntryDetailText(entry),
                        fontFamily = FontFamily.Monospace,
                    )
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(android.R.string.ok))
            }
        },
    )
}
