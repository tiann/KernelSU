package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.screen.about.AboutScreenActions
import me.weishu.kernelsu.ui.screen.about.AboutUiState
import me.weishu.kernelsu.ui.screen.sulog.SulogActions
import me.weishu.kernelsu.ui.screen.sulog.SulogScreenState
import me.weishu.kernelsu.ui.screen.sulog.buildSulogFileSelector
import me.weishu.kernelsu.ui.screen.sulog.sulogEntryDescription
import me.weishu.kernelsu.ui.screen.sulog.sulogEntryDetailText
import me.weishu.kernelsu.ui.screen.sulog.sulogEntryStatus
import me.weishu.kernelsu.ui.screen.sulog.sulogEntrySummaryTags
import me.weishu.kernelsu.ui.screen.sulog.sulogEntryTitle
import me.weishu.kernelsu.ui.screen.sulog.sulogFilterLabel
import me.weishu.kernelsu.ui.util.SulogEntry
import me.weishu.kernelsu.ui.util.SulogEventFilter
import ui.feice.mochi.basic.MochiBarIconButton
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.MochiChip
import ui.feice.mochi.basic.MochiChipGroup
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogScrollContent
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiEmptyState
import ui.feice.mochi.basic.MochiSearchBar
import ui.feice.mochi.basic.Text
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.mochiListSection
import ui.feice.mochi.list.rows
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold
import ui.feice.mochi.shape.MochiRoundedRectangle
import ui.feice.mochi.theme.MochiTheme

@Composable
fun AboutScreenMochi(state: AboutUiState, actions: AboutScreenActions) {
    MochiLazyLargeTitleScaffold(title = state.title, navigationIcon = { MochiBackButton() }) {
        item(key = "hero") {
            Column(Modifier.fillMaxWidth().padding(vertical = 12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Box(
                    Modifier.size(88.dp).clip(MochiRoundedRectangle(22.dp)).background(Color.White),
                    contentAlignment = Alignment.Center,
                ) {
                    Image(painterResource(R.drawable.ic_launcher_foreground), contentDescription = null, modifier = Modifier.size(88.dp))
                }
                Text(state.appName, modifier = Modifier.padding(top = 14.dp), style = MochiTheme.textStyles.titleLarge)
                Text(state.versionName, style = MochiTheme.textStyles.subhead, color = MochiTheme.colorScheme.onSurfaceVariant)
            }
        }
        item(key = "links") {
            MochiListSection {
                state.links.forEach { link ->
                    val github = link.url.contains("github", ignoreCase = true)
                    MochiListRow(
                        link.fullText,
                        subtitle = link.url.removePrefix("https://"),
                        leading = { KsuTile(if (github) "logo-github" else "paper-plane", if (github) KsuTileColors.Gray else KsuTileColors.Blue) },
                        chevron = true,
                        onClick = { actions.onOpenLink(link.url) },
                    )
                }
            }
        }
        item(key = "mochi") {
            MochiListSection(footer = stringResource(R.string.mochi_about_project_summary)) {
                MochiListRow(
                    stringResource(R.string.mochi_about_project),
                    subtitle = MochiProjectUrl.removePrefix("https://"),
                    leading = { MochiLogoTile() },
                    chevron = true,
                    onClick = { actions.onOpenLink(MochiProjectUrl) },
                )
            }
        }
    }
}

internal const val MochiProjectUrl ="https://github.com/feice11/mochi-compose"

@Composable
fun SulogScreenMochi(state: SulogScreenState, actions: SulogActions) {
    val refreshing = rememberUpdatedState(state.isLoading || state.isRefreshing)
    var selected by remember { mutableStateOf<SulogEntry?>(null) }
    var confirmClean by remember { mutableStateOf(false) }
    val selector = buildSulogFileSelector(state.files, state.selectedFilePath)
    val cleanTitle = stringResource(R.string.sulog_clean_title)
    val cs = MochiTheme.colorScheme
    val banner: Pair<String, Boolean>? = when (state.sulogStatus) {
        "unsupported" -> stringResource(R.string.sulog_unsupported_title) to false
        "managed" -> stringResource(R.string.feature_status_managed_summary) to false
        "forced" -> stringResource(R.string.feature_status_forced_summary) to false
        "supported" -> if (!state.isSulogEnabled) stringResource(R.string.sulog_disabled_title) to true else null
        else -> null
    }

    MochiLazyCompactTitleScaffold(
        title = stringResource(R.string.settings_sulog),
        navigationIcon = { MochiBackButton() },
        actions = {
            MochiBarIconButton(
                onClick = { confirmClean = true },
                enabled = state.selectedFilePath != null,
                modifier = Modifier.semantics { contentDescription = cleanTitle },
                navId = "sulog.clean",
                contentKey = "trash",
            ) { MochiIonicon("trash") }
        },
        searchBar = {
            MochiSearchBar(query = state.searchText, onQueryChange = actions.onSearchTextChange, onCancel = { actions.onSearchTextChange("") })
        },
        onRefresh = { awaitRefresh(actions.onRefresh, refreshing) },
    ) {
        banner?.let { (message, canEnable) ->
            item(key = "banner") {
                MochiListSection {
                    MochiListRow(
                        message,
                        leading = { MochiIonicon("alert-circle", tint = cs.error) },
                        leadingStyle = ui.feice.mochi.list.MochiListLeadingStyle.Symbol,
                        trailing = if (canEnable) {
                            {
                                MochiButton(onClick = actions.onEnableSulog, style = MochiButtonStyle.ProminentGlass) {
                                    Text(stringResource(R.string.sulog_enable_action))
                                }
                            }
                        } else {
                            null
                        },
                    )
                }
            }
        }
        item(key = "file") {
            MochiListSection(header = stringResource(R.string.sulog_filter_title)) {
                MochiListPickerRow(
                    stringResource(R.string.sulog_log_files),
                    options = selector.items.toImmutableList(),
                    selectedIndex = selector.selectedIndex.coerceAtLeast(0),
                    onSelect = { index -> state.files.getOrNull(index)?.let { actions.onSelectFile(it.path) } },
                    enabled = selector.items.isNotEmpty(),
                )
                // Clip the chip row to its own bounds so chips do not scroll past the card.
                MochiChipGroup(Modifier.padding(vertical = 12.dp)) {
                    SulogEventFilter.entries.forEach { filter ->
                        MochiChip(
                            sulogFilterLabel(filter),
                            selected = filter in state.selectedFilters,
                            onClick = { actions.onToggleFilter(filter) },
                            showCheck = true,
                        )
                    }
                }
            }
        }
        when {
            state.errorMessage != null -> item(key = "error") {
                MochiEmptyState(
                    stringResource(R.string.sulog_failed_to_load),
                    message = state.errorMessage,
                    icon = { MochiIonicon("warning", size = 44.dp, tint = cs.error) },
                )
            }

            state.visibleEntries.isEmpty() && !state.isLoading -> item(key = "empty") {
                MochiEmptyState(stringResource(R.string.mochi_no_results), icon = { MochiIonicon("reader", size = 44.dp) })
            }

            else -> mochiListSection(key = "entries") {
                rows(stableSulogKeys(state.visibleEntries), key = { it.first }) { (_, entry) ->
                    val tags = sulogEntrySummaryTags(entry)
                    val subtitle = listOfNotNull(sulogEntryDescription(entry), tags.joinToString("  ").takeIf { it.isNotBlank() })
                        .joinToString("\n")
                    MochiListRow(
                        sulogEntryTitle(entry),
                        subtitle = subtitle.ifBlank { null },
                        value = entry.timestampText,
                        status = sulogEntryStatus(entry)?.let { status ->
                            {
                                val ok = status == "Success"
                                ui.feice.mochi.list.MochiListStatusChip(
                                    status,
                                    containerColor = if (ok) cs.primaryContainer else cs.errorContainer,
                                    contentColor = if (ok) cs.onPrimaryContainer else cs.onErrorContainer,
                                )
                            }
                        },
                        onClick = { selected = entry },
                    )
                }
            }
        }
    }

    MochiDialog(
        visible = confirmClean,
        onDismissRequest = { confirmClean = false },
        title = cleanTitle,
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), ui.feice.mochi.basic.MochiDialogActionRole.Cancel),
            MochiDialogAction(stringResource(android.R.string.ok), ui.feice.mochi.basic.MochiDialogActionRole.Destructive, onClick = actions.onCleanFile),
        ),
    )
    val shown = selected
    MochiDialog(
        visible = shown != null,
        onDismissRequest = { selected = null },
        title = shown?.let { sulogEntryTitle(it) } ?: "",
        actions = persistentListOf(MochiDialogAction(stringResource(android.R.string.ok))),
    ) {
        shown?.let {
            MochiDialogScrollContent(maxHeight = 420.dp) {
                androidx.compose.foundation.text.BasicText(
                    sulogEntryDetailText(it),
                    style = MochiTheme.textStyles.footnote.copy(fontFamily = FontFamily.Monospace, color = cs.onSurface),
                )
            }
        }
    }
}

/**
 * Stable row keys: the entry key plus how many identical entries come after it (counted from the
 * oldest end). New log lines arrive at the top, so keys of existing rows do not change on refresh;
 * an index-based key shifted every row and made the whole list fade out and back in.
 */
private fun stableSulogKeys(entries: List<SulogEntry>): List<Pair<String, SulogEntry>> {
    val seen = HashMap<String, Int>()
    val keys = arrayOfNulls<String>(entries.size)
    for (i in entries.indices.reversed()) {
        val k = entries[i].key
        val n = seen.getOrDefault(k, 0)
        seen[k] = n + 1
        keys[i] = "$k#$n"
    }
    return entries.mapIndexed { i, e -> keys[i]!! to e }
}
