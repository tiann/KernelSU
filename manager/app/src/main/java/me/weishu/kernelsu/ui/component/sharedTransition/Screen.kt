package me.weishu.kernelsu.ui.component.sharedTransition

import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.EnterExitState
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.animation.SharedTransitionScope.PlaceholderSize.Companion.ContentSize
import androidx.compose.animation.SharedTransitionScope.ResizeMode.Companion.scaleToBounds
import androidx.compose.animation.core.animateDp
import androidx.compose.animation.core.tween
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousRoundedRectangle
import me.weishu.kernelsu.ui.component.getSystemCornerRadius
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
                    EnterExitState.PreEnter, EnterExitState.PostExit -> when (transitionSource) {
                        TransitionSource.FAB -> 30.dp
                        TransitionSource.LIST_CARD -> CardDefaults.CornerRadius
                    }

                    EnterExitState.Visible -> getSystemCornerRadius()
                }
            }
            val resizeMode: SharedTransitionScope.ResizeMode = when (transitionSource) {
                TransitionSource.FAB -> scaleToBounds(ContentScale.FillBounds, Alignment.TopCenter)
                TransitionSource.LIST_CARD -> scaleToBounds(ContentScale.FillWidth, Alignment.TopCenter)
            }

            Modifier
                .skipToLookaheadSize()
                .sharedBounds(
                    sharedContentState = rememberSharedContentState(key = "$transitionSource/$key"),
                    animatedVisibilityScope = animatedVisibilityScope,
                    resizeMode = resizeMode,
                    clipInOverlayDuringTransition = OverlayClip(ContinuousRoundedRectangle(pagerCorner.value)),
                    placeholderSize = ContentSize,
                    boundsTransform = defaultBoundsTransform
                )
        }
    )
}