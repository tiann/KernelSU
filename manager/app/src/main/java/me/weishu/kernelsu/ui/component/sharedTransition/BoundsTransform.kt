package me.weishu.kernelsu.ui.component.sharedTransition

import androidx.compose.animation.BoundsTransform
import androidx.compose.animation.core.tween
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.NavAnimationEasing
import me.weishu.kernelsu.ui.component.navigation.MiuixNavHostDefaults.SHARETRANSITION_DURATION

val defaultBoundsTransform = BoundsTransform { initialBounds, targetBounds ->
    tween(SHARETRANSITION_DURATION, 0, NavAnimationEasing)
}
