package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshotFlow
import androidx.compose.runtime.withFrameNanos
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.time.Duration.Companion.milliseconds

private const val LIST_QUIET_MILLIS = 250L
private const val LIST_SETTLE_TIMEOUT_MS = 3000L

private class KeysRef {
    var value: List<Any?>? = null
}

/**
 * Flicker-free scroll-to-top when [keys] (sort/filter/search/refresh token) change while the
 * composition is alive. Never scrolls on first composition or nav-return (NavDisplay recreates the
 * screen and resets `remember`), so returning from a detail won't auto-scroll. For actions that
 * don't change sort/filter (e.g. pull-to-refresh) pass a token that only increments then.
 *
 * [isBusy]: pass `{ isRefreshing }` for slow reloads so pinning lasts until the reload finishes;
 * read it via rememberUpdatedState (uiState is a plain value param).
 */
@Composable
fun ScrollToTopOnChange(
    listState: LazyListState,
    vararg keys: Any?,
    onScrolledToTop: () -> Unit = {},
    isBusy: () -> Boolean = { false },
    observedList: () -> Any?,
) {
    val keysList = keys.asList()
    val sideRef = remember { KeysRef() }
    SideEffect {
        val prev = sideRef.value
        if (prev != null && prev != keysList) {
            listState.requestScrollToItem(0)
        }
        sideRef.value = keysList
    }
    val effectRef = remember { KeysRef() }
    LaunchedEffect(*keys) {
        val prev = effectRef.value
        effectRef.value = keysList
        if (prev != null && prev != keysList) {
            scrollToTopAfterListSettles(listState, isBusy = isBusy, observedList = observedList)
            onScrolledToTop()
        }
    }
}

/**
 * Pins the list to the top each frame until [observedList] is quiet for [LIST_QUIET_MILLIS] and
 * [isBusy]() is false (capped by [LIST_SETTLE_TIMEOUT_MS]), overriding LazyColumn's key-based
 * position restore. [observedList] must read a snapshot-trackable State (bridge uiState via
 * rememberUpdatedState), otherwise settle detection falls back to a fixed timeout.
 */
suspend fun scrollToTopAfterListSettles(
    listState: LazyListState,
    isBusy: () -> Boolean = { false },
    observedList: () -> Any?,
): Unit = coroutineScope {
    var settled = false
    val watcher = launch {
        while (true) {
            val changed = withTimeoutOrNull(LIST_QUIET_MILLIS.milliseconds) {
                snapshotFlow(observedList).drop(1).first()
            }
            if (changed == null && !isBusy()) break
        }
        settled = true
    }
    withTimeoutOrNull(LIST_SETTLE_TIMEOUT_MS.milliseconds) {
        while (!settled) {
            listState.requestScrollToItem(0)
            withFrameNanos { }
        }
    }
    watcher.cancel()
    listState.requestScrollToItem(0)
}
