package me.weishu.kernelsu.ui.component.bottombar

import androidx.compose.animation.core.AnimationVector1D
import androidx.compose.animation.core.TargetBasedAnimation
import androidx.compose.animation.core.VectorConverter
import androidx.compose.animation.core.spring
import androidx.compose.foundation.MutatePriority
import androidx.compose.foundation.pager.PagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.job
import kotlinx.coroutines.launch
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import top.yukonga.miuix.kmp.blur.Backdrop
import top.yukonga.miuix.kmp.blur.LayerBackdrop
import kotlin.math.abs

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
                pagerState.springAnimateToPage(targetIndex)
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

private suspend fun PagerState.springAnimateToPage(target: Int) {
    if (target !in 0 until pageCount) return
    scroll(MutatePriority.UserInput) {
        val pageSize = layoutInfo.pageSize + layoutInfo.pageSpacing
        val distance = target - currentPage - currentPageOffsetFraction
        val scrollPixels = distance * pageSize
        if (abs(scrollPixels) <= 0.5f) return@scroll

        val animation = TargetBasedAnimation<Float, AnimationVector1D>(
            animationSpec = spring(
                stiffness = 322.2f,
                dampingRatio = 32.31f / (2f * kotlin.math.sqrt(322.2f)),
                visibilityThreshold = 0.5f,
            ),
            typeConverter = Float.VectorConverter,
            initialValue = 0f,
            targetValue = scrollPixels,
            initialVelocityVector = AnimationVector1D(0f),
        )

        var current = 0f
        var lastFrameNanos = 0L
        var playTimeNanos = 0L
        var finished = false

        withFrameNanos { lastFrameNanos = it }
        while (!finished) {
            withFrameNanos { frameNanos ->
                playTimeNanos += frameNanos - lastFrameNanos
                lastFrameNanos = frameNanos

                val currentValue = animation.getValueFromNanos(playTimeNanos)
                val currentVelocity = animation.getVelocityVectorFromNanos(playTimeNanos).value
                val delta = currentValue - current

                if (abs(delta) > 0.5f) {
                    val consumed = scrollBy(delta)
                    current += consumed
                    if (abs(delta - consumed) > 0.1f) {
                        finished = true
                    }
                } else {
                    current = currentValue
                }

                if (abs(currentVelocity) < 0.1f && abs(scrollPixels - current) < 1.0f) {
                    finished = true
                } else if (animation.isFinishedFromNanos(playTimeNanos)) {
                    finished = true
                }
            }
        }

        val remaining = scrollPixels - current
        if (abs(remaining) > 0.5f) {
            current += scrollBy(remaining)
        }

        if (currentPage != target) {
            scrollToPage(target)
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
