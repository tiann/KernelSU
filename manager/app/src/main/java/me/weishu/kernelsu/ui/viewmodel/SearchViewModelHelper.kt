package me.weishu.kernelsu.ui.viewmodel

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.FlowPreview
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.debounce
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import me.weishu.kernelsu.ui.component.SearchStatus

private const val SEARCH_DEBOUNCE_MILLIS = 150L

@OptIn(FlowPreview::class)
fun CoroutineScope.launchSearchQueryCollector(
    searchQuery: StateFlow<String>,
    onQuery: suspend (String) -> Unit,
): Job {
    return launch {
        searchQuery
            .debounce(SEARCH_DEBOUNCE_MILLIS)
            .distinctUntilChanged()
            .collectLatest(onQuery)
    }
}

fun searchLoadingStatusFor(text: String): SearchStatus.ResultStatus {
    return if (text.isEmpty()) {
        SearchStatus.ResultStatus.DEFAULT
    } else {
        SearchStatus.ResultStatus.LOAD
    }
}

fun searchResultStatusFor(text: String, isEmpty: Boolean): SearchStatus.ResultStatus {
    return when {
        text.isEmpty() -> SearchStatus.ResultStatus.DEFAULT
        isEmpty -> SearchStatus.ResultStatus.EMPTY
        else -> SearchStatus.ResultStatus.SHOW
    }
}
