package me.weishu.kernelsu.ui.screen.sulog

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.itemsIndexed
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Card
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.SulogEventFilter
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearPrimaryButton
import me.weishu.kernelsu.ui.wear.WearSearchField
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun SulogScreenWear(
    state: SulogScreenState,
    actions: SulogActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()
    val fileSelector = buildSulogFileSelector(state.files, state.selectedFilePath)

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.settings_sulog))
                }
            }

            item {
                WearTonalButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onRefresh,
                    label = stringResource(R.string.pull_down_refresh),
                )
            }

            item {
                WearSearchField(
                    value = state.searchText,
                    placeholder = stringResource(R.string.wear_search_logs),
                    onValueChange = actions.onSearchTextChange,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                )
            }

            if (state.searchText.isNotEmpty()) {
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = { actions.onSearchTextChange("") },
                        label = stringResource(R.string.delete),
                    )
                }
            }

            if (fileSelector.items.size > 1 && fileSelector.selectedIndex >= 0) {
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = {
                            val nextIndex =
                                (fileSelector.selectedIndex + 1) % fileSelector.items.size
                            state.files.getOrNull(nextIndex)?.path?.let(actions.onSelectFile)
                        },
                        label = stringResource(R.string.sulog_log_files),
                        secondaryLabel = fileSelector.items[fileSelector.selectedIndex],
                    )
                }
            }

            SulogEventFilter.entries.forEach { filter ->
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = { actions.onToggleFilter(filter) },
                        label = sulogFilterLabel(filter),
                        secondaryLabel = if (filter in state.selectedFilters) stringResource(R.string.wear_state_on) else stringResource(
                            R.string.wear_state_off
                        ),
                    )
                }
            }

            if (!state.isSulogEnabled) {
                item {
                    WearPrimaryButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onEnableSulog,
                        label = stringResource(R.string.sulog_enable),
                    )
                }
            }

            if (state.sulogStatus.isNotEmpty()) {
                item {
                    WearMessageText(
                        text = state.sulogStatus,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            }

            if (state.isLoading || state.isRefreshing) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.processing),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else if (state.visibleEntries.isEmpty()) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.sulog_empty),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else {
                itemsIndexed(state.visibleEntries, key = { index, entry -> "$index-${entry.key}" }) { _, entry ->
                    val title = sulogEntryTitle(entry)
                    val desc = sulogEntryDescription(entry)
                    Card(
                        onClick = {},
                        modifier = Modifier
                            .padding(horizontal = horizontalPadding)
                            .fillMaxWidth(),
                    ) {
                        Text(text = title, style = MaterialTheme.typography.titleSmall)
                        if (desc != null) {
                            Text(
                                text = desc,
                                style = MaterialTheme.typography.bodySmall,
                                maxLines = 2,
                            )
                        }
                        val status = sulogEntryStatus(entry)
                        if (status != null) {
                            Text(
                                text = status,
                                style = MaterialTheme.typography.labelSmall,
                            )
                        }
                    }
                }
            }
        }
    }
}
