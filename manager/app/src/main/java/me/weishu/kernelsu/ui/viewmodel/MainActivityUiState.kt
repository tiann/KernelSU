package me.weishu.kernelsu.ui.viewmodel

import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.theme.AppSettings

data class MainActivityUiState(
    val appSettings: AppSettings,
    val pageScale: Float,
    val enableBlur: Boolean,
    val enableFloatingBottomBar: Boolean,
    val enableFloatingBottomBarBlur: Boolean,
    val uiMode: UiMode,
)
