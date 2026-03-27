package me.weishu.kernelsu.ui.util

import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import dev.chrisbanes.haze.ExperimentalHazeApi
import dev.chrisbanes.haze.HazeInputScale
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.hazeEffect

@OptIn(ExperimentalHazeApi::class)
fun Modifier.defaultHazeEffect(
    hazeState: HazeState,
    hazeStyle: HazeStyle,
): Modifier = this.hazeEffect(
    state = hazeState,
    style = hazeStyle
) {
    blurRadius = 20.dp
    inputScale = HazeInputScale.Fixed(0.35f)
    noiseFactor = 0f
    forceInvalidateOnPreDraw = false
}
