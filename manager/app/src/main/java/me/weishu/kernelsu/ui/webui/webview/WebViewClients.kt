package me.weishu.kernelsu.ui.webui.webview

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.RenderProcessGoneDetail
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.AppIconCache
import me.weishu.kernelsu.ui.util.withMainUserUid
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import me.weishu.kernelsu.ui.webui.WebUIRuntime
import me.weishu.kernelsu.ui.webui.model.WebUIDialog
import me.weishu.kernelsu.ui.webui.viewmodel.WebUIViewModel

@SuppressLint("MissingOnRenderProcessGone")
internal class WebUiClient(
    private val activity: Activity,
    private val webViewAssetLoader: WebViewAssetLoader,
    private val viewModel: WebUIViewModel,
) : WebViewClient() {

    override fun onRenderProcessGone(view: WebView?, detail: RenderProcessGoneDetail?): Boolean {
        viewModel.notifyError(activity.getString(R.string.webui_render_process_crashed))
        return true
    }

    override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
        val url = request.url
        if (url.scheme.equals("ksu", ignoreCase = true) && url.host.equals("icon", ignoreCase = true)) {
            val packageName = url.path?.runCatching { substring(1) }?.getOrNull()
            if (!packageName.isNullOrEmpty()) {
                val appInfo = SuperUserViewModel.apps.find { it.packageName == packageName }?.packageInfo?.applicationInfo
                if (appInfo != null) {
                    val icon = AppIconCache.loadIconSync(activity, appInfo.withMainUserUid(activity), 512)
                    val stream = java.io.ByteArrayOutputStream()
                    icon.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, stream)
                    return WebResourceResponse(
                        "image/png", null, 200, "OK", CORS_HEADERS, java.io.ByteArrayInputStream(stream.toByteArray())
                    )
                } else {
                    return notFoundResponse("No such package")
                }
            }
        }
        return webViewAssetLoader.shouldInterceptRequest(url)
    }

    override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
        viewModel.onHistoryChanged(view?.canGoBack() ?: false)
        super.doUpdateVisitedHistory(view, url, isReload)
    }
}

internal class WebUiChromeClient(
    private val runtime: WebUIRuntime,
    private val viewModel: WebUIViewModel,
) : WebChromeClient() {
    override fun onJsAlert(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
        if (message == null || result == null) return false
        viewModel.showDialog(
            WebUIDialog.Alert(
                message = message,
                onDismiss = {
                    viewModel.clearDialog()
                    result.cancel()
                },
                onConfirm = {
                    viewModel.clearDialog()
                    result.confirm()
                }
            )
        )
        return true
    }

    override fun onJsConfirm(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
        if (message == null || result == null) return false
        viewModel.showDialog(
            WebUIDialog.Confirm(
                message = message,
                onDismiss = {
                    viewModel.clearDialog()
                    result.cancel()
                },
                onConfirm = {
                    viewModel.clearDialog()
                    result.confirm()
                }
            )
        )
        return true
    }

    override fun onJsPrompt(
        view: WebView?, url: String?, message: String?, defaultValue: String?, result: JsPromptResult?
    ): Boolean {
        if (message == null || result == null) return false
        viewModel.showDialog(
            WebUIDialog.Prompt(
                message = message,
                defaultValue = defaultValue ?: "",
                onDismiss = {
                    viewModel.clearDialog()
                    result.cancel()
                },
                onConfirm = { text ->
                    viewModel.clearDialog()
                    result.confirm(text)
                }
            )
        )
        return true
    }

    override fun onShowFileChooser(
        webView: WebView?, filePathCallback: ValueCallback<Array<Uri>>?, fileChooserParams: FileChooserParams?
    ): Boolean {
        if (filePathCallback == null) return false

        val intent = fileChooserParams?.createIntent() ?: Intent(Intent.ACTION_GET_CONTENT).apply { type = "*/*" }
        if (fileChooserParams?.mode == FileChooserParams.MODE_OPEN_MULTIPLE) {
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
        }
        val requestId = runtime.beginFileChooser(intent, filePathCallback)
        viewModel.requestFileChooser(requestId)
        return true
    }
}

internal fun notFoundResponse(msg: String = "") = WebResourceResponse(
    "text/plain", "utf-8", 404, "Not Found", CORS_HEADERS, java.io.ByteArrayInputStream(msg.toByteArray())
)

private val CORS_HEADERS = mapOf("Access-Control-Allow-Origin" to "*")
