package me.weishu.kernelsu.ui.component

import androidx.compose.animation.core.Transition

val <T> Transition<T>.isTransitioning: Boolean
    get() = currentState != targetState