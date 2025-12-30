package me.weishu.kernelsu.ui.component.navigation

import android.util.Log
import androidx.compose.animation.EnterExitState
import androidx.compose.animation.animateColor
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousRoundedRectangle
import com.ramcosta.composedestinations.manualcomposablecalls.ManualComposableCallsBuilder
import com.ramcosta.composedestinations.manualcomposablecalls.composable
import com.ramcosta.composedestinations.scope.AnimatedDestinationScope
import com.ramcosta.composedestinations.spec.DestinationStyle
import com.ramcosta.composedestinations.spec.TypedDestinationSpec
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import me.weishu.kernelsu.ui.component.getCornerRadiusTop
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.NavAnimationEasing
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.SHARETRANSITION_DURATION

fun <T> ManualComposableCallsBuilder.miuixComposable(
    destination: TypedDestinationSpec<T>,
    animation: DestinationStyle.Animated? =  null ,
    content: @Composable AnimatedDestinationScope<T>.() -> Unit
) {
    animation?.let {
        destination.animateWith(it)
    }
    composable(destination){
        val desTransition = this.transition
        val currentState = desTransition.currentState
        val targetState = desTransition.targetState
        val isPop = routePopupState.getValue(destination.route.substringBefore('/'))
        val screenCornerRadius = getCornerRadiusTop()

        Log.d("miuixComposable", "${destination.route.substringBefore('/')}: $isPop")

        with(desTransition){
            val dim = animateColor({ tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing) }) { enterExitState ->
                when(enterExitState){
                    EnterExitState.Visible ->{
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
            val screenRadius = remember(currentState,targetState) {
                derivedStateOf {
                    if (currentState == targetState) 0.dp else screenCornerRadius
                }
            }

            Box(
                modifier = Modifier.drawWithContent {
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
                        modifier = Modifier.clip(ContinuousRoundedRectangle(screenRadius.value))
                    ) {
                        this@composable.content()
                    }
                    Box(modifier = Modifier.background(dim.value))
                }
            }
        }
    }
}
