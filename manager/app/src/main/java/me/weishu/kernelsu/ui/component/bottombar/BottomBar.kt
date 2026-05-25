package me.weishu.kernelsu.ui.component.bottombar

import androidx.compose.foundation.MutatePriority
import androidx.compose.foundation.pager.PagerState
import androidx.compose.runtime.withFrameNanos
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import top.yukonga.miuix.kmp.blur.Backdrop
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.job
import kotlinx.coroutines.launch
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import top.yukonga.miuix.kmp.blur.LayerBackdrop
import kotlin.math.abs

private suspend fun PagerState.springScrollToPage(target: Int) {
    scroll(MutatePriority.UserInput) {
        val tension = 322.2f
        val damping = 32.31f
        val pageSize = layoutInfo.pageSize + layoutInfo.pageSpacing
        val distance = target - currentPage - currentPageOffsetFraction
        val scrollPixels = distance * pageSize
        var current = 0f
        var velocity = 0f
        var lastNanos = 0L
        var finished = false
        withFrameNanos { lastNanos = it }
        while (!finished) {
            withFrameNanos { frameNanos ->
                val dt = ((frameNanos - lastNanos) / 1e9f).coerceAtMost(0.016f)
                lastNanos = frameNanos
                velocity = velocity * (1f - damping * dt) +
                        tension * (scrollPixels - current) * dt
                val newPos = current + dt * velocity
                val delta = newPos - current
                if (abs(delta) > 0.5f) {
                    val consumed = scrollBy(delta)
                    current += consumed
                    if (abs(delta - consumed) > 0.1f) {
                        finished = true
                    }
                } else {
                    current = newPos
                }
                if (abs(velocity) < 0.1f && abs(scrollPixels - current) < 1.0f) {
                    finished = true
                }
            }
        }
    }
    if (target in 0 until pageCount) {
        scrollToPage(target)
    }
}

class MainPagerState(
    val pagerState: PagerState,
    private val coroutineScope: CoroutineScope
) {
    var selectedPage by mutableIntStateOf(pagerState.currentPage)
        private set

    var isNavigating by mutableStateOf(false)
        private set

    private var navJob: Job? = null

    fun animateToPage(targetIndex: Int) {
        if (targetIndex == selectedPage) return

        navJob?.cancel()

        selectedPage = targetIndex
        isNavigating = true

        navJob = coroutineScope.launch {
            val myJob = coroutineContext.job
            try {
                pagerState.springScrollToPage(targetIndex)
            } finally {
                if (navJob == myJob) {
                    isNavigating = false
                    if (pagerState.currentPage != targetIndex) {
                        selectedPage = pagerState.currentPage
                    }
                }
            }
        }
    }

    fun syncPage() {
        if (!isNavigating && selectedPage != pagerState.currentPage) {
            selectedPage = pagerState.currentPage
        }
    }
}

@Composable
fun rememberMainPagerState(
    pagerState: PagerState,
    coroutineScope: CoroutineScope = rememberCoroutineScope()
): MainPagerState {
    return remember(pagerState, coroutineScope) {
        MainPagerState(pagerState, coroutineScope)
    }
}

@Composable
fun BottomBar(
    blurBackdrop: LayerBackdrop?,
    backdrop: Backdrop,
    modifier: Modifier = Modifier,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> BottomBarMiuix(blurBackdrop, backdrop, modifier)
        UiMode.Material -> BottomBarMaterial()
    }
}

@Composable
fun SideRail(
    blurBackdrop: LayerBackdrop?,
    modifier: Modifier = Modifier,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> NavigationRailMiuix(blurBackdrop, modifier)
        UiMode.Material -> NavigationRailMaterial(modifier)
    }
}
