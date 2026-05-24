package me.weishu.kernelsu.ui.component.material

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.Easing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.offset
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarData
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.AccessibilityManager
import androidx.compose.ui.platform.LocalAccessibilityManager
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

private const val ANIMATION_DURATION = 450
private const val ANIMATION_FADE_DURATION = 200

val LocalSnackbarHost = compositionLocalOf<SnackbarHostState> {
    error("CompositionLocal LocalSnackBarController not present")
}

class OvershootEasing(private val tension: Float = 2.0f) : Easing {
    override fun transform(fraction: Float): Float {
        val t = fraction - 1.0f
        return t * t * ((tension + 1) * t + tension) + 1.0f
    }
}

private fun getSnackBarEnterTransition() = slideInVertically(
    initialOffsetY = { fullHeight -> fullHeight }, animationSpec = tween(
        durationMillis = ANIMATION_DURATION, easing = OvershootEasing()
    )
) + scaleIn(
    initialScale = 1.1f, animationSpec = tween(
        durationMillis = ANIMATION_DURATION, easing = FastOutSlowInEasing
    )
) + fadeIn(
    animationSpec = tween(durationMillis = ANIMATION_FADE_DURATION)
)

private fun getSnackBarExitTransition() = slideOutVertically(
    targetOffsetY = { fullHeight -> fullHeight }, animationSpec = tween(durationMillis = ANIMATION_DURATION, easing = FastOutSlowInEasing)
) + fadeOut(
    animationSpec = tween(durationMillis = ANIMATION_FADE_DURATION)
)


@Composable
private fun FadeInFadeOutWithScale(
    current: SnackbarData?,
    modifier: Modifier = Modifier,
    content: @Composable (SnackbarData) -> Unit,
) {
    AnimatedContent(
        targetState = current,
        transitionSpec = {
            (getSnackBarEnterTransition() togetherWith getSnackBarExitTransition()).using(sizeTransform = null)
        },
        modifier = modifier,
        label = "breeze_snackbar_animation"
    ) { data ->
        if (data != null) {
            content(data)
        }
    }
}

@Composable
fun SnackBarHost(
    hostState: SnackbarHostState,
    modifier: Modifier = Modifier,
    snackBar: @Composable (SnackbarData) -> Unit = { SnackBar(it) },
) {
    val currentSnackBarData = hostState.currentSnackbarData
    val accessibilityManager = LocalAccessibilityManager.current
    LaunchedEffect(currentSnackBarData) {
        if (currentSnackBarData != null) {
            val duration = currentSnackBarData.visuals.duration.toMillis(
                currentSnackBarData.visuals.actionLabel != null,
                accessibilityManager,
            )
            delay(duration)
            currentSnackBarData.dismiss()
        }
    }
    FadeInFadeOutWithScale(
        current = hostState.currentSnackbarData,
        modifier = modifier,
        content = snackBar,
    )
}

private fun SnackbarDuration.toMillis(
    hasAction: Boolean,
    accessibilityManager: AccessibilityManager?,
): Long {
    val original = when (this) {
        SnackbarDuration.Indefinite -> Long.MAX_VALUE
        SnackbarDuration.Long -> 10000L
        SnackbarDuration.Short -> 4000L
    }
    if (accessibilityManager == null) {
        return original
    }
    return accessibilityManager.calculateRecommendedTimeoutMillis(
        original,
        containsIcons = true,
        containsText = true,
        containsControls = hasAction,
    )
}

@Composable
fun SnackBar(
    snackBarData: SnackbarData,
    modifier: Modifier = Modifier
) {
    val scope = rememberCoroutineScope()
    val density = LocalDensity.current
    val offsetY = remember(snackBarData) { Animatable(0f) }
    // SingleLineContainerHeight
    val dismissThresholdPx = remember(density) {
        with(density) { 48.dp.toPx() }
    }

    Snackbar(
        snackbarData = snackBarData,
        modifier = modifier
            .offset {
                IntOffset(0, offsetY.value.roundToInt())
            }
            .pointerInput(snackBarData) {
                detectVerticalDragGestures(
                    onDragEnd = {
                        if (offsetY.value > dismissThresholdPx) {
                            snackBarData.dismiss()
                        } else {
                            scope.launch {
                                offsetY.animateTo(0f)
                            }
                        }
                    },
                    onDragCancel = {
                        scope.launch {
                            offsetY.animateTo(0f)
                        }
                    }
                ) { change, dragAmount ->
                    change.consume()
                    val newOffset = (offsetY.value + dragAmount).coerceAtLeast(0f)
                    scope.launch {
                        offsetY.snapTo(newOffset)
                    }
                }
            }
    )
}
