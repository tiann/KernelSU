package me.weishu.kernelsu.ui.component.sharedTransition

import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.BoundsTransform
import androidx.compose.animation.EnterExitState
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.animation.SharedTransitionScope.PlaceholderSize.Companion.ContentSize
import androidx.compose.animation.SharedTransitionScope.ResizeMode.Companion.scaleToBounds
import androidx.compose.animation.core.animateDp
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousRoundedRectangle
import me.weishu.kernelsu.ui.component.getCornerRadiusTop
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.NavAnimationEasing
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.SHARETRANSITION_DURATION
import top.yukonga.miuix.kmp.basic.CardDefaults

@Composable
fun Modifier.screenShareBounds(
    key: Any,
    transitionSource: TransitionSource,
    sharedTransitionScope: SharedTransitionScope?,
    animatedVisibilityScope: AnimatedVisibilityScope,
): Modifier {
    return this.then(
        with(sharedTransitionScope) {
            if (this == null || transitionSource == TransitionSource.NULL) return Modifier

            val pagerCorner = animatedVisibilityScope.transition.animateDp(
                {
                    tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)
                }) { enterExitState ->
                when (enterExitState) {
                    EnterExitState.PreEnter, EnterExitState.PostExit -> when(transitionSource) {
                        TransitionSource.FAB -> 30.dp
                        TransitionSource.LIST_CARD -> CardDefaults.CornerRadius
                        else -> 0.dp
                    }
                    EnterExitState.Visible -> getCornerRadiusTop()
                }
            }
            val resizeMode: SharedTransitionScope.ResizeMode = when(transitionSource) {
                TransitionSource.FAB -> scaleToBounds(ContentScale.FillBounds, Alignment.TopCenter)
                TransitionSource.LIST_CARD -> scaleToBounds(ContentScale.FillWidth, Alignment.TopCenter)
                else -> scaleToBounds(ContentScale.FillWidth, Alignment.TopCenter)
            }
            val exitDurationMillis = when(transitionSource) {
                TransitionSource.FAB -> SHARETRANSITION_DURATION
                TransitionSource.LIST_CARD -> SHARETRANSITION_DURATION/3*2
                else -> 0
            }

            Modifier.sharedBounds(
                sharedContentState = rememberSharedContentState(key = "$transitionSource/$key"),
                animatedVisibilityScope = animatedVisibilityScope,
                resizeMode = resizeMode,
                clipInOverlayDuringTransition = OverlayClip(ContinuousRoundedRectangle(pagerCorner.value)),
                placeholderSize = ContentSize,
                boundsTransform = BoundsTransform { _, _ ->
                    tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)
                },
                enter = fadeIn(tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)),
                exit = fadeOut(tween(exitDurationMillis, 0, NavAnimationEasing))
            )
        }
    )
}