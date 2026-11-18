package me.weishu.kernelsu.ui.mochi

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshotFlow
import androidx.compose.runtime.snapshots.Snapshot
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import top.yukonga.miuix.kmp.nav.core.NavKey
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import ui.feice.mochi.navigation.MochiNavHost
import ui.feice.mochi.navigation.rememberMochiNavigator

private const val RootRoute = "root"

/**
 * Connects the manager [Navigator] to MochiNavHost:
 * - pushes and pops on the manager stack are mirrored to the Mochi stack
 * - back gestures in MochiNavHost pop the manager stack
 *
 * Mochi routes are "index + route" strings on top of [RootRoute]. Pages that are closing are looked up in a cache.
 */
@Composable
fun MochiKsuNavHost(
    navigator: Navigator,
    predictiveBack: Boolean,
    modifier: Modifier = Modifier,
    content: @Composable (NavKey) -> Unit,
) {
    val cache = remember { HashMap<String, NavKey>() }
    val mochi = rememberMochiNavigator(RootRoute)

    fun routeOf(index: Int, key: NavKey): String =
        if (index == 0) RootRoute else "$index:${key.hashCode()}".also { cache[it] = key }

    // manager stack -> Mochi stack
    LaunchedEffect(navigator, mochi) {
        snapshotFlow { navigator.backStack.mapIndexed(::routeOf) }.collect { target ->
            val current = mochi.backStack
            var common = 0
            while (common < current.size && common < target.size && current[common] == target[common]) common++
            common = common.coerceAtLeast(1)
            Snapshot.withMutableSnapshot {
                when {
                    current.size - common == 1 -> mochi.pop()
                    current.size - common > 1 -> mochi.popTo(common - 1)
                }
                for (i in common until target.size) mochi.push(target[i])
            }
        }
    }

    // Mochi stack -> manager stack
    LaunchedEffect(navigator, mochi) {
        snapshotFlow { mochi.backStack.size }.collect { size ->
            val stack = navigator.backStack
            if (size != mochi.backStack.size || size >= stack.size) return@collect
            val top = stack.lastOrNull()
            if (size == stack.size - 1 && top is Route.TemplateEditor && !top.readOnly) {
                // Pass the edited result back like NavDisplay onBack does
                navigator.setResult("template_edit", true)
            } else {
                while (navigator.backStack.size > size.coerceAtLeast(1)) navigator.pop()
            }
        }
    }

    MochiNavHost(mochi, modifier, predictiveBack = predictiveBack) { route ->
        val key = if (route == RootRoute) {
            navigator.backStack.firstOrNull() ?: Route.Main
        } else {
            cache[route] ?: route.substringBefore(':').toIntOrNull()?.let { navigator.backStack.getOrNull(it) } ?: Route.Main
        }
        content(key)
    }
}

/**
 * Blocks back while flashing or running a script, including the edge swipe of MochiNavHost.
 */
@Composable
fun MochiBackGuard(enabled: Boolean, content: @Composable () -> Unit) {
    BackHandler(enabled = !enabled) { }
    Box(
        Modifier
            .fillMaxSize()
            .then(
                if (enabled) {
                    Modifier
                } else {
                    Modifier.pointerInput(Unit) {
                        awaitEachGesture {
                            awaitFirstDown(requireUnconsumed = false)
                            do {
                                val event = awaitPointerEvent(PointerEventPass.Main)
                                event.changes.forEach { it.consume() }
                            } while (event.changes.any { it.pressed })
                        }
                    }
                },
            ),
    ) { content() }
}
