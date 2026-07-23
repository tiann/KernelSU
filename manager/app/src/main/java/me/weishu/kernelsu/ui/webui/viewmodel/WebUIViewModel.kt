package me.weishu.kernelsu.ui.webui.viewmodel

import android.webkit.JsPromptResult
import android.webkit.JsResult
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import me.weishu.kernelsu.ui.webui.model.WebUIEffect
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUILoadState
import me.weishu.kernelsu.ui.webui.model.WebUIOverlay
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.runtime.WebUIRuntime

class WebUIViewModel : ViewModel() {
    val runtime = WebUIRuntime()

    private val _state = MutableStateFlow(WebUIState())
    val state: StateFlow<WebUIState> = _state.asStateFlow()

    private val _effect = Channel<WebUIEffect>(Channel.BUFFERED)
    val effect = _effect.receiveAsFlow()

    private var pendingJsResult: JsResult? = null
    private var pendingJsPromptResult: JsPromptResult? = null

    fun dispatch(intent: WebUIIntent) {
        when (intent) {
            is WebUIIntent.ModuleReady -> {
                runtime.rootShell = intent.shell
                runtime.webView = intent.webView
                _state.update {
                    it.copy(
                        loadState = WebUILoadState.Ready,
                        isUrlLoaded = false,
                        webCanGoBack = false,
                        overlay = null,
                        externalLinkUrl = null,
                    )
                }
            }

            is WebUIIntent.Error -> emitEffects(WebUIEffect.ShowToast(intent.message), WebUIEffect.Finish)

            WebUIIntent.ExitRequested -> emitEffect(WebUIEffect.Finish)

            WebUIIntent.HomePageLoaded -> _state.update { it.copy(isUrlLoaded = true) }

            is WebUIIntent.HistoryChanged -> _state.update { it.copy(webCanGoBack = intent.canGoBack) }

            is WebUIIntent.InsetsEnabledChanged -> _state.update { it.copy(isInsetsEnabled = intent.enabled) }

            is WebUIIntent.InsetsChanged -> {
                val oldState = _state.value
                if (oldState.currentInsets != intent.insets) {
                    _state.update { it.copy(currentInsets = intent.insets) }
                    emitEffect(WebUIEffect.EvaluateJavascript(intent.insets.js))
                }
            }

            is WebUIIntent.JsAlertRequested -> {
                pendingJsResult = intent.result
                _state.update { it.copy(overlay = WebUIOverlay.Alert(intent.message)) }
            }

            is WebUIIntent.JsConfirmRequested -> {
                pendingJsResult = intent.result
                _state.update { it.copy(overlay = WebUIOverlay.Confirm(intent.message)) }
            }

            is WebUIIntent.JsPromptRequested -> {
                pendingJsPromptResult = intent.result
                _state.update { it.copy(overlay = WebUIOverlay.Prompt(intent.message, intent.defaultValue)) }
            }

            WebUIIntent.AlertConfirmed -> {
                pendingJsResult?.confirm()
                pendingJsResult = null
                _state.update { it.copy(overlay = null) }
            }

            is WebUIIntent.ConfirmAnswered -> {
                if (intent.confirmed) pendingJsResult?.confirm() else pendingJsResult?.cancel()
                pendingJsResult = null
                _state.update { it.copy(overlay = null) }
            }

            is WebUIIntent.PromptAnswered -> {
                if (intent.value != null) pendingJsPromptResult?.confirm(intent.value) else pendingJsPromptResult?.cancel()
                pendingJsPromptResult = null
                _state.update { it.copy(overlay = null) }
            }

            is WebUIIntent.FileChooserRequested -> {
                emitEffect(WebUIEffect.LaunchFileChooser(intent.intent))
            }

            is WebUIIntent.FileChooserResult -> {
                runtime.pendingFileCallback?.onReceiveValue(intent.uris)
                runtime.pendingFileCallback = null
            }

            is WebUIIntent.ExternalLinkIntercepted -> _state.update {
                it.copy(externalLinkUrl = intent.url)
            }

            WebUIIntent.ExternalLinkGoBack -> _state.update {
                it.copy(externalLinkUrl = null)
            }

            is WebUIIntent.ExternalLinkOpenBrowser -> {
                _state.update { it.copy(externalLinkUrl = null) }
                emitEffect(WebUIEffect.OpenExternalBrowser(intent.url))
            }
        }
    }

    private fun emitEffect(effect: WebUIEffect) {
        viewModelScope.launch {
            _effect.send(effect)
        }
    }

    private fun emitEffects(vararg effects: WebUIEffect) {
        viewModelScope.launch {
            for (effect in effects) {
                _effect.send(effect)
            }
        }
    }

    override fun onCleared() {
        runtime.dispose()
    }
}
