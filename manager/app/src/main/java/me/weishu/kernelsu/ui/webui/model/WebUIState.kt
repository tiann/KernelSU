package me.weishu.kernelsu.ui.webui.model

import androidx.compose.runtime.Immutable

@Immutable
sealed interface WebUILoadState {
    data object Idle : WebUILoadState
    data object LoadingModule : WebUILoadState
    data object LoadingPage : WebUILoadState
    data object Ready : WebUILoadState
}

@Immutable
data class WebUIState(
    val loadState: WebUILoadState = WebUILoadState.Idle,
    val moduleInfo: ModuleInfo? = null,
    val activeDialog: WebUIDialog? = null,
    val isEdgeToEdge: Boolean = false,
    val isInsetsSupported: Boolean = true,
    val webViewVersion: Int = 0,
    val webViewPackageName: String = "",
    val banners: List<WebUIBanner> = emptyList(),
    val shownBannerIds: Set<String> = emptySet(),
    val webCanGoBack: Boolean = false,
) {
    val isLoading: Boolean get() = loadState != WebUILoadState.Ready
}
