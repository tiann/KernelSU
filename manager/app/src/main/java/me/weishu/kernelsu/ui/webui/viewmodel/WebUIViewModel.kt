package me.weishu.kernelsu.ui.webui.viewmodel

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import me.weishu.kernelsu.ui.webui.model.ModuleInfo
import me.weishu.kernelsu.ui.webui.model.WebUIBanner
import me.weishu.kernelsu.ui.webui.model.WebUIDialog
import me.weishu.kernelsu.ui.webui.model.WebUIEffect
import me.weishu.kernelsu.ui.webui.model.WebUILoadState
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.util.WebViewVersionUtil

class WebUIViewModel : ViewModel() {
    private val _state = MutableStateFlow(WebUIState())
    val state: StateFlow<WebUIState> = _state.asStateFlow()

    private val _effect = Channel<WebUIEffect>(Channel.BUFFERED)
    val effect = _effect.receiveAsFlow()

    fun checkWebviewFeature(context: Context) {
        val majorVersion = WebViewVersionUtil.getWebViewMajorVersion(context)
        val packageName = WebViewVersionUtil.getWebViewPackageName(context)
        val isSupported = WebViewVersionUtil.supportInsets(majorVersion)

        _state.update {
            it.copy(
                webViewVersion = majorVersion,
                webViewPackageName = packageName,
                isInsetsSupported = isSupported,
            )
        }
    }

    fun isModuleValidated(moduleId: String): Boolean =
        moduleId == _state.value.moduleInfo?.id

    fun onModuleValidated(moduleInfo: ModuleInfo) {
        _state.update { it.copy(moduleInfo = moduleInfo) }
    }

    fun recreateWebView() {
        _state.update {
            it.copy(
                activeDialog = null,
                webCanGoBack = false,
                loadState = WebUILoadState.LoadingPage,
            )
        }
    }

    fun setLoadState(loadState: WebUILoadState) {
        _state.update { it.copy(loadState = loadState) }
    }

    fun showDialog(dialog: WebUIDialog) {
        _state.update { it.copy(activeDialog = dialog) }
    }

    fun clearDialog() {
        _state.update { it.copy(activeDialog = null) }
    }

    fun dismissBanner(id: String) {
        _state.update { it.copy(banners = it.banners.filterNot { b -> b.id == id }) }
    }

    fun updateWebView() {
        val pkg = _state.value.webViewPackageName.ifBlank { WebViewVersionUtil.DEFAULT_WEBVIEW_PACKAGE }
        emitEffect(WebUIEffect.OpenExternalLink("market://details?id=$pkg"))
    }

    fun onHomePageLoaded() {
        if (_state.value.loadState != WebUILoadState.Ready) {
            _state.update { it.copy(loadState = WebUILoadState.Ready) }
        }
    }

    fun onHistoryChanged(canGoBack: Boolean) {
        _state.update { it.copy(webCanGoBack = canGoBack) }
    }

    fun onEdgeToEdgeChanged(enable: Boolean) {
        val bannerId = WebUIBanner.OutdatedWebView.ID
        _state.update { state ->
            // The banner is announced at most once per session: the id is recorded when it is
            // shown, so neither a page reload nor a re-request can bring it back or duplicate it.
            val shouldShowBanner = enable &&
                    !state.isInsetsSupported &&
                    bannerId !in state.shownBannerIds

            if (!shouldShowBanner) {
                state.copy(isEdgeToEdge = enable)
            } else {
                state.copy(
                    isEdgeToEdge = true,
                    banners = state.banners + WebUIBanner.OutdatedWebView(
                        currentVersion = state.webViewVersion,
                        requiredVersion = WebViewVersionUtil.MIN_INSETS_VERSION,
                    ),
                    shownBannerIds = state.shownBannerIds + bannerId,
                )
            }
        }
    }

    fun notifyError(message: String) {
        emitEffects(WebUIEffect.ShowToast(message), WebUIEffect.Finish)
    }

    fun toast(message: String) {
        emitEffect(WebUIEffect.ShowToast(message))
    }

    fun exit() {
        emitEffect(WebUIEffect.Finish)
    }

    fun openExternalLink(url: String) {
        emitEffect(WebUIEffect.OpenExternalLink(url))
    }

    fun requestFileChooser(requestId: Long) {
        emitEffect(WebUIEffect.RequestFileChooser(requestId))
    }

    private fun emitEffect(effect: WebUIEffect) {
        viewModelScope.launch {
            _effect.send(effect)
        }
    }

    private fun emitEffects(vararg effects: WebUIEffect) {
        viewModelScope.launch {
            for (e in effects) {
                _effect.send(e)
            }
        }
    }
}
