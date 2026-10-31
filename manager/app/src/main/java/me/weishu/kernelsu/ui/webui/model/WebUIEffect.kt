package me.weishu.kernelsu.ui.webui.model

sealed interface WebUIEffect {
    data class ShowToast(val message: String) : WebUIEffect
    data object Finish : WebUIEffect
    data class RequestFileChooser(val requestId: Long) : WebUIEffect
    data class OpenExternalLink(val url: String) : WebUIEffect
}
