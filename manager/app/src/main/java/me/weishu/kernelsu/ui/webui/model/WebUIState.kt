package me.weishu.kernelsu.ui.webui.model

import androidx.compose.runtime.Immutable

@Immutable
data class WebUIState(
    val loadState: WebUILoadState = WebUILoadState.Loading,
    val isUrlLoaded: Boolean = false,
    val currentInsets: Insets = Insets(0, 0, 0, 0),
    val isInsetsEnabled: Boolean = false,
    val webCanGoBack: Boolean = false,
    val overlay: WebUIOverlay? = null,
    val externalLinkUrl: String? = null,
)

@Immutable
sealed interface WebUILoadState {
    data object Loading : WebUILoadState
    data object Ready : WebUILoadState
}

@Immutable
sealed interface WebUIOverlay {
    data class Alert(val message: String) : WebUIOverlay
    data class Confirm(val message: String) : WebUIOverlay
    data class Prompt(val message: String, val defaultValue: String) : WebUIOverlay
}
