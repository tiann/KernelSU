package me.weishu.kernelsu.ui.screen.sulog

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.util.SulogEntry
import me.weishu.kernelsu.ui.util.SulogEventFilter
import me.weishu.kernelsu.ui.util.SulogEventType
import me.weishu.kernelsu.ui.util.SulogFile
import me.weishu.kernelsu.ui.util.toSulogDisplayName
import me.weishu.kernelsu.ui.viewmodel.SulogViewModel

@Composable
fun SulogScreen() {
    val navigator = LocalNavigator.current
    val uiMode = LocalUiMode.current
    val viewModel = viewModel<SulogViewModel>()
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()

    LaunchedEffect(Unit) {
        viewModel.refreshLatest()
    }

    val state = SulogScreenState(
        isLoading = uiState.isLoading,
        isRefreshing = uiState.isRefreshing,
        sulogStatus = uiState.sulogStatus,
        isSulogEnabled = uiState.isSulogEnabled,
        searchText = uiState.searchText,
        selectedFilters = uiState.selectedFilters,
        files = uiState.files,
        selectedFilePath = uiState.selectedFilePath,
        entries = uiState.entries,
        visibleEntries = uiState.visibleEntries,
        errorMessage = uiState.errorMessage,
    )
    val actions = SulogActions(
        onBack = dropUnlessResumed { navigator.pop() },
        onRefresh = viewModel::refreshLatest,
        onEnableSulog = viewModel::enableSulog,
        onCleanFile = viewModel::cleanFile,
        onSearchTextChange = viewModel::setSearchText,
        onToggleFilter = viewModel::toggleFilter,
        onSelectFile = viewModel::refresh,
    )

    when (uiMode) {
        UiMode.Material -> SulogScreenMaterial(state, actions)
        UiMode.Miuix -> SulogScreenMiuix(state, actions)
    }
}

@Composable
fun sulogFilterLabel(filter: SulogEventFilter): String {
    return when (filter) {
        SulogEventFilter.RootExecve -> stringResource(R.string.sulog_filter_root_execve)
        SulogEventFilter.SuCompat -> stringResource(R.string.sulog_filter_sucompat)
        SulogEventFilter.IoctlGrantRoot -> stringResource(R.string.sulog_filter_ioctl_grant_root)
        SulogEventFilter.DaemonEvent -> stringResource(R.string.sulog_filter_daemon_restart)
    }
}

@Composable
fun sulogEntryTitle(entry: SulogEntry): String {
    return when (entry.eventType) {
        SulogEventType.RootExecve -> entry.fields["comm"] ?: stringResource(R.string.sulog_filter_root_execve)
        SulogEventType.SuCompat -> stringResource(R.string.sulog_filter_sucompat)
        SulogEventType.IoctlGrantRoot -> stringResource(R.string.sulog_filter_ioctl_grant_root)
        SulogEventType.DaemonEvent -> stringResource(R.string.sulog_filter_daemon_restart)
        SulogEventType.Dropped -> "Dropped"
        SulogEventType.Unknown -> entry.fields["type"]?.replace('_', ' ')?.replaceFirstChar(Char::uppercase) ?: "Unknown"
    }
}

@Composable
fun sulogEntryDescription(entry: SulogEntry): String? {
    return when (entry.eventType) {
        SulogEventType.DaemonEvent -> entry.fields["boot_id"]?.let { "Boot ID: $it" }
        SulogEventType.Dropped -> entry.fields["ts_ns"]?.let { "Timestamp: $it" }
        else -> entry.fields["argv"] ?: entry.fields["file"]
    }
}

fun sulogEntrySummaryTags(entry: SulogEntry): List<String> {
    val comm = entry.fields["comm"]
    val pid = entry.fields["pid"]
    val uid = entry.fields["uid"]
    return when (entry.eventType) {
        SulogEventType.DaemonEvent -> listOfNotNull(entry.fields["restart"]?.let { "Restart #$it" } ?: "Daemon restarted")
        SulogEventType.Dropped -> listOfNotNull(entry.fields["dropped"]?.let { "$it lost" })
        else -> listOfNotNull(comm?.takeIf { it.isNotBlank() }, pid?.let { "PID $it" }, uid?.let { "UID $it" })
    }
}

fun sulogEntryDetailText(entry: SulogEntry) = buildAnnotatedString {
    entry.fields.entries.forEachIndexed { index, (key, value) ->
        if (index > 0) append('\n')
        withStyle(SpanStyle(fontWeight = FontWeight.Bold)) {
            append("$key: ")
        }
        append(value)
    }
}

fun sulogEntryStatus(entry: SulogEntry): String? {
    return entry.fields["retval"]?.toIntOrNull()?.let(::formatSulogStatus)
}

private fun formatSulogStatus(retval: Int): String {
    return if (retval == 0) "Success" else "Exit $retval"
}

fun buildSulogFileSelector(
    files: List<SulogFile>,
    selectedFilePath: String?,
): SulogFileSelector {
    if (files.isEmpty()) {
        return SulogFileSelector(
            items = emptyList(),
            selectedIndex = -1,
        )
    }

    val selectedIndex = files.indexOfFirst { it.path == selectedFilePath }
        .takeIf { it >= 0 }
        ?: 0

    return SulogFileSelector(
        items = files.map { it.name.toSulogDisplayName() },
        selectedIndex = selectedIndex,
    )
}
