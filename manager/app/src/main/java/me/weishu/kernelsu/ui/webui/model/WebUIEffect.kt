package me.weishu.kernelsu.ui.webui.model

import android.content.Intent

sealed interface WebUIEffect {
    data class ShowToast(val message: String) : WebUIEffect
    data object Finish : WebUIEffect
    data class LaunchFileChooser(val intent: Intent) : WebUIEffect
    data class EvaluateJavascript(val script: String) : WebUIEffect
    data class OpenExternalBrowser(val url: String) : WebUIEffect
}
