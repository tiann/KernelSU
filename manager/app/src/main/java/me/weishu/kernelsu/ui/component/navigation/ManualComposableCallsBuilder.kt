package me.weishu.kernelsu.ui.component.navigation

import androidx.compose.animation.EnterExitState
import androidx.compose.animation.animateColor
import androidx.compose.animation.core.animateDp
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousRoundedRectangle
import com.ramcosta.composedestinations.manualcomposablecalls.composable
import com.ramcosta.composedestinations.scope.AnimatedDestinationScope
import com.ramcosta.composedestinations.spec.DestinationStyle
import com.ramcosta.composedestinations.spec.TypedDestinationSpec
import dev.chrisbanes.haze.hazeEffect
import me.weishu.kernelsu.ui.component.getSystemCornerRadius
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.NavAnimationEasing
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.SHARETRANSITION_DURATION
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

fun <T> MiuixManualComposableCallsBuilder.miuixComposable(
    destination: TypedDestinationSpec<T>,
    animation: DestinationStyle.Animated? = null,
    popTransitionStyle: PopTransitionStyle = PopTransitionStyle.None,
    content: @Composable AnimatedDestinationScope<T>.() -> Unit
) {
    val (destinationsNavigator, routePopupState) = this
    destinationsNavigator.run {
        animation?.let {
            destination.animateWith(it)
        }
        composable(destination) {
            val desTransition = this.transition
            val currentState = desTransition.currentState
            val targetState = desTransition.targetState
            val isPop = routePopupState.getValue(destination.route.substringBefore('/'))
            val screenCornerRadius = getSystemCornerRadius()

            with(desTransition) {
                val dim = animateColor({ tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing) }) { enterExitState ->
                    when (enterExitState) {
                        EnterExitState.Visible -> {
                            Color.Transparent
                        }

                        else -> {
                            if (isPop) {
                                colorScheme.windowDimming
                            } else {
                                Color.Transparent
                            }
                        }
                    }
                }
                val screenRadius = remember(currentState, targetState) {
                    derivedStateOf { if (currentState == targetState) 0.dp else screenCornerRadius }
                }

                val popModifier = when (popTransitionStyle) {
                    PopTransitionStyle.Depth -> {
                        val blur = animateDp({ tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing) }) { enterExitState ->
                            when (enterExitState) {
                                EnterExitState.PreEnter, EnterExitState.PostExit -> if (isPop) 20.dp else 0.dp
                                EnterExitState.Visible -> 0.dp
                            }
                        }
                        val scale = animateFloat({ tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing) }) { enterExitState ->
                            when (enterExitState) {
                                EnterExitState.PreEnter, EnterExitState.PostExit -> if (isPop) 0.88f else 1f
                                EnterExitState.Visible -> 1f
                            }
                        }
                        Modifier
                            .hazeEffect {
                                noiseFactor = 0f
                                blurEnabled = blur.value != 0.dp
                                drawContentBehind = drawContentBehind
                                blurRadius = blur.value.coerceAtLeast(0.dp)
                            }
                            .scale(scale.value)
                    }

                    else -> Modifier
                }

                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .drawWithContent {
                            drawContent()
                            drawRect(
                                color = dim.value,
                                size = size
                            )
                        }
                ) {
                    CompositionLocalProvider(
                        LocalAnimatedVisibilityScope provides this@composable,
                        localPopState provides isPop
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .clip(ContinuousRoundedRectangle(screenRadius.value))
                                .then(popModifier)
                        ) {
                            this@composable.content()
                        }
                    }
                }
            }
        }
    }
}
