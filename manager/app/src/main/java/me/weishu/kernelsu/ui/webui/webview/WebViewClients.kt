package me.weishu.kernelsu.ui.webui.webview

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import me.weishu.kernelsu.ui.util.AppIconCache
import me.weishu.kernelsu.ui.util.withMainUserUid
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.runtime.WebUIRuntime


internal fun createWebViewClient(
    activity: Activity,
    webViewAssetLoader: WebViewAssetLoader,
    runtime: WebUIRuntime,
    getState: () -> WebUIState,
    dispatch: (WebUIIntent) -> Unit,
): WebViewClient {
    return object : WebViewClient() {
        override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
            val url = request.url
            if (url.scheme.equals("ksu", ignoreCase = true) && url.host.equals("icon", ignoreCase = true)) {
                val packageName = url.path?.substring(1)
                if (!packageName.isNullOrEmpty()) {
                    val appInfo = SuperUserViewModel.apps
                        .find { it.packageName == packageName }
                        ?.packageInfo?.applicationInfo
                    if (appInfo != null) {
                        val icon = AppIconCache.loadIconSync(activity, appInfo.withMainUserUid(activity), 512)
                        val stream = java.io.ByteArrayOutputStream()
                        icon.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, stream)
                        return WebResourceResponse(
                            "image/png", null, 200, "OK",
                            mapOf("Access-Control-Allow-Origin" to "*"),
                            java.io.ByteArrayInputStream(stream.toByteArray())
                        )
                    } else {
                        val errorMsg = "No such package"
                        val errorStream = java.io.ByteArrayInputStream(errorMsg.toByteArray(Charsets.UTF_8))
                        return WebResourceResponse(
                            "text/plain",
                            "utf-8",
                            404,
                            "Not Found",
                            mapOf("Access-Control-Allow-Origin" to "*"),
                            errorStream
                        )
                    }
                }
            }
            return webViewAssetLoader.shouldInterceptRequest(url)
        }

        override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
            dispatch(WebUIIntent.HistoryChanged(view?.canGoBack() ?: false))
            val state = getState()
            if (state.isInsetsEnabled) runtime.webView?.evaluateJavascript(state.currentInsets.js, null)
            super.doUpdateVisitedHistory(view, url, isReload)
        }
    }
}

internal fun createWebChromeClient(
    runtime: WebUIRuntime,
    dispatch: (WebUIIntent) -> Unit,
): WebChromeClient {
    return object : WebChromeClient() {
        override fun onJsAlert(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
            if (message == null || result == null) return false
            dispatch(WebUIIntent.JsAlertRequested(message, result))
            return true
        }

        override fun onJsConfirm(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
            if (message == null || result == null) return false
            dispatch(WebUIIntent.JsConfirmRequested(message, result))
            return true
        }

        override fun onJsPrompt(
            view: WebView?,
            url: String?,
            message: String?,
            defaultValue: String?,
            result: JsPromptResult?
        ): Boolean {
            if (message == null || result == null || defaultValue == null) return false
            dispatch(WebUIIntent.JsPromptRequested(message, defaultValue, result))
            return true
        }

        override fun onShowFileChooser(
            webView: WebView?, filePathCallback: ValueCallback<Array<Uri>>?, fileChooserParams: FileChooserParams?
        ): Boolean {
            if (filePathCallback == null) return false
            runtime.pendingFileCallback?.onReceiveValue(null)
            runtime.pendingFileCallback = filePathCallback

            val intent = fileChooserParams?.createIntent() ?: Intent(Intent.ACTION_GET_CONTENT).apply { type = "*/*" }
            if (fileChooserParams?.mode == FileChooserParams.MODE_OPEN_MULTIPLE) {
                intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
            }
            dispatch(WebUIIntent.FileChooserRequested(intent))
            return true
        }
    }
}
