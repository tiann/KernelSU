package me.weishu.kernelsu.ui.component.sharedTransition

import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.BoundsTransform
import androidx.compose.animation.EnterExitState
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.animation.SharedTransitionScope.PlaceholderSize.Companion.AnimatedSize
import androidx.compose.animation.SharedTransitionScope.PlaceholderSize.Companion.ContentSize
import androidx.compose.animation.SharedTransitionScope.ResizeMode.Companion.RemeasureToBounds
import androidx.compose.animation.core.animateDp
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import com.kyant.capsule.ContinuousRoundedRectangle
import me.weishu.kernelsu.ui.component.getCornerRadiusTop
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.NavAnimationEasing
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.SHARETRANSITION_DURATION
import top.yukonga.miuix.kmp.basic.CardDefaults

@Composable
fun Modifier.cardShareBounds(
    key: Any,
    sharedTransitionScope: SharedTransitionScope?,
    animatedVisibilityScope: AnimatedVisibilityScope,
    cardRadius : Dp = CardDefaults.CornerRadius
): Modifier {
    return this.then(
        with(sharedTransitionScope) {
            if (this == null) return Modifier

            val pagerCorner: State<Dp> = animatedVisibilityScope.transition.animateDp({
                tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)
            }) { enterExitState ->
                when (enterExitState) {
                    EnterExitState.PreEnter, EnterExitState.PostExit -> getCornerRadiusTop()

                    EnterExitState.Visible -> with(LocalDensity.current) {
                        cardRadius
                    }
                }

            }

            Modifier.sharedBounds(
                sharedContentState = rememberSharedContentState(key = "${TransitionSource.LIST_CARD}/${key}"),
                animatedVisibilityScope = animatedVisibilityScope,
                resizeMode = RemeasureToBounds,
                clipInOverlayDuringTransition = OverlayClip(ContinuousRoundedRectangle(pagerCorner.value)),
                placeholderSize = ContentSize,
                boundsTransform = BoundsTransform { _, _ ->
                    tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)
                },
                enter = fadeIn(tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)),
                exit = fadeOut(tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing))
            )
        }
    )
}