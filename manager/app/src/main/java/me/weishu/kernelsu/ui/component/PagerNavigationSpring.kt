package me.weishu.kernelsu.ui.component

import androidx.compose.animation.core.SpringSpec
import androidx.compose.animation.core.spring

/**
 * Spring spec shared by pager tab navigation
 */
internal val PagerNavigationSpringSpec: SpringSpec<Float> = spring(
    stiffness = 322.2f,
    dampingRatio = 32.31f / (2f * kotlin.math.sqrt(322.2f)),
    visibilityThreshold = 0.5f,
)
