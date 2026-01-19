package me.weishu.kernelsu.ui.navigation3

import androidx.navigation3.runtime.NavKey
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow

/**
 * Simple navigation helper that owns a back stack and result channels.
 * Supports push/replace/pop/popUntil and result APIs: navigateForResult/setResult/observeResult/clearResult.
 */
class Navigator(
    val backStack: MutableList<NavKey>
) {
    private val resultBus = mutableMapOf<String, MutableSharedFlow<Any>>()

    /**
     * Push a key onto the back stack.
     */
    fun push(key: NavKey) {
        backStack.add(key)
    }

    /**
     * Replace the top key, or push if the stack is empty.
     */
    fun replace(key: NavKey) {
        if (backStack.isNotEmpty()) {
            backStack[backStack.lastIndex] = key
        } else {
            backStack.add(key)
        }
    }

    /**
     * Pop the top key if present.
     */
    fun pop() {
        backStack.removeLastOrNull()
    }

    /**
     * Pop until predicate matches the top key.
     */
    fun popUntil(predicate: (NavKey) -> Boolean) {
        while (backStack.isNotEmpty() && !predicate(backStack.last())) {
            backStack.removeAt(backStack.lastIndex)
        }
    }

    /**
     * Navigate expecting a result. Caller should subscribe via observeResult(requestKey).
     */
    fun navigateForResult(route: Route, requestKey: String) {
        ensureChannel(requestKey)
        push(route)
    }

    /**
     * Set a result for the given request and then pop.
     */
    fun <T : Any> setResult(requestKey: String, value: T) {
        ensureChannel(requestKey).tryEmit(value)
        pop()
    }

    /**
     * Observe results for a given request key as a SharedFlow.
     */
    @Suppress("UNCHECKED_CAST")
    fun <T : Any> observeResult(requestKey: String): SharedFlow<T> {
        return ensureChannel(requestKey) as SharedFlow<T>
    }

    /**
     * Clear the last emitted result for the request key.
     */
    @OptIn(ExperimentalCoroutinesApi::class)
    fun clearResult(requestKey: String) {
        ensureChannel(requestKey).resetReplayCache()
    }

    private fun ensureChannel(key: String): MutableSharedFlow<Any> {
        return resultBus.getOrPut(key) { MutableSharedFlow(replay = 1, extraBufferCapacity = 0) }
    }
}
