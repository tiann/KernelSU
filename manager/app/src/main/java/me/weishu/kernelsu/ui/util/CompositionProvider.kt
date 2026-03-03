package me.weishu.kernelsu.ui.util

import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.compositionLocalOf

val LocalSnackbarHost = compositionLocalOf<SnackbarHostState> {
    error("CompositionLocal LocalSnackbarHost not present")
}

val LocalShowSwitchIcon = compositionLocalOf { false }
