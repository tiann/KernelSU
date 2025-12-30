package me.weishu.kernelsu.ui.component.navigation

import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.Easing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.runtime.Immutable
import androidx.navigation.NavBackStackEntry
import com.ramcosta.composedestinations.animations.NavHostAnimatedDestinationStyle
import com.ramcosta.composedestinations.spec.DestinationSpec
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.NavAnimationEasing
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.SHARETRANSITION_DURATION
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.TRANSITION_DURATION
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

@Immutable
class NavTransitionEasing @JvmOverloads constructor(
    response: Float = 0.3f,
    damping: Float = 0.85f
) : Easing {
    private val c: Float
    private val w: Float
    private val r: Float
    private val c2: Float

    init {
        val k = (6.283185307179586 / response).pow(2.0).toFloat()
        c = ((damping * 12.566370614359172) / response).toFloat()
        w = sqrt((4.0f * k) - (c * c)) / 2.0f
        r = -(c / 2.0f)
        c2 = (r * 1.0f) / w
    }

    override fun transform(fraction: Float): Float {
        return ((2.718281828459045.pow(r * fraction.toDouble()) * ((-1.0f * cos(w * fraction)) + (c2 * sin(w * fraction)))) + 1.0).toFloat()
    }

}
fun defaultTransitions() = object : NavHostAnimatedDestinationStyle() {
    override val enterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition =
        {
            slideInHorizontally(
                initialOffsetX = { it },
                animationSpec = tween(TRANSITION_DURATION, 0, NavAnimationEasing)
            )
        }

    override val exitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition =
        {
            slideOutHorizontally(
                targetOffsetX = { -it / 5 },
                animationSpec = tween(TRANSITION_DURATION, 0, NavAnimationEasing)
            )
        }

    override val popEnterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition =
        {
            slideInHorizontally(
                initialOffsetX = { -it / 5 },
                animationSpec = tween(TRANSITION_DURATION, 0, NavAnimationEasing)
            )
        }

    override val popExitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition =
        {
            slideOutHorizontally(
                targetOffsetX = { it },
                animationSpec = tween(TRANSITION_DURATION, 0, NavAnimationEasing)
            )
        }
    override val sizeTransform: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> SizeTransform?) = { null }
}

val noAnimated =  object : NavHostAnimatedDestinationStyle() {
    override val enterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition = { EnterTransition.None }
    override val exitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition = { ExitTransition.None }
    override val popEnterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition = { EnterTransition.None }
    override val popExitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition = { ExitTransition.None }
    override val sizeTransform: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> SizeTransform?) = { null }
}

val slideFromRightTransition =  object : NavHostAnimatedDestinationStyle() {
    override val enterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition = {
        slideInHorizontally(
            initialOffsetX = { it },
            animationSpec = tween(TRANSITION_DURATION, 0, NavAnimationEasing)
        )
    }
    override val exitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition = { ExitTransition.None }
    override val popEnterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition = { EnterTransition.None }
    override val popExitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition = {
        slideOutHorizontally(
            targetOffsetX = { it },
            animationSpec = tween(TRANSITION_DURATION, 0, NavAnimationEasing)
        )
    }
    override val sizeTransform: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> SizeTransform?) = { null }
}
