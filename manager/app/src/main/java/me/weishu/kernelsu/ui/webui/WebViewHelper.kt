package me.weishu.kernelsu.ui.webui

import android.annotation.SuppressLint
import android.app.Activity
import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.repository.ModuleRepositoryImpl
import me.weishu.kernelsu.ui.util.AppIconCache
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.util.withMainUserUid
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import java.io.File

private fun isTrustedWebUiUrl(uri: Uri): Boolean =
    isTrustedWebUiOrigin(uri.scheme, uri.host, uri.port)

private fun isSupportedExternalUri(uri: Uri): Boolean =
    isSupportedExternalScheme(uri.scheme)

private fun openExternalUri(activity: Activity, uri: Uri) {
    if (!isSupportedExternalUri(uri)) return
    runCatching {
        activity.startActivity(
            Intent(Intent.ACTION_VIEW, uri).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        )
    }
}

fun Activity.setTaskDescription(label: String) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
        @Suppress("DEPRECATION")
        setTaskDescription(ActivityManager.TaskDescription(label))
    } else {
        val taskDescription = ActivityManager.TaskDescription.Builder()
            .setLabel(label)
            .build()
        setTaskDescription(taskDescription)
    }
}

@SuppressLint("SetJavaScriptEnabled")
internal suspend fun prepareWebView(
    activity: Activity,
    moduleId: String,
    webUIState: WebUIState,
) {
    withContext(Dispatchers.IO) {
        val repo = ModuleRepositoryImpl()
        val modules = repo.getModules().getOrDefault(emptyList())
        val moduleInfo = modules.find { info -> info.id == moduleId }

        if (moduleInfo == null) {
            withContext(Dispatchers.Main) {
                webUIState.uiEvent = WebUIEvent.Error(activity.getString(R.string.no_such_module, moduleId))
            }
            return@withContext
        }

        if (!moduleInfo.hasWebUi || !moduleInfo.enabled || moduleInfo.update || moduleInfo.remove) {
            withContext(Dispatchers.Main) {
                webUIState.uiEvent = WebUIEvent.Error(activity.getString(R.string.module_unavailable, moduleInfo.name))
            }
            return@withContext
        }

        webUIState.moduleName = moduleInfo.name
        webUIState.modDir = "/data/adb/modules/${moduleId}"

        if (SuperUserViewModel.apps.isEmpty()) {
            SuperUserViewModel().fetchAppList()
        }
        val shell = createRootShell(true)
        webUIState.rootShell = shell

        withContext(Dispatchers.Main) {
            activity.setTaskDescription(activity.getString(R.string.app_name) + " - ${moduleInfo.name}")

            val webView = WebView(activity)
            webView.setBackgroundColor(Color.TRANSPARENT)

            val prefs = activity.getSharedPreferences("settings", Context.MODE_PRIVATE)
            WebView.setWebContentsDebuggingEnabled(prefs.getBoolean("enable_web_debugging", false))

            webView.settings.apply {
                javaScriptEnabled = true
                domStorageEnabled = true
                mixedContentMode = android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW
                allowFileAccess = false
                allowContentAccess = false
            }

            val webRoot = File("${webUIState.modDir}/webroot")
            val webViewAssetLoader = WebViewAssetLoader.Builder()
                .setDomain(WEBUI_DOMAIN)
                .addPathHandler(
                    "/",
                    SuFilePathHandler(
                        activity,
                        webRoot,
                        shell,
                        { webUIState.currentInsets },
                        { enable -> webUIState.isInsetsEnabled = enable })
                )
                .build()

            // WebViewClient
            webView.webViewClient = object : WebViewClient() {
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
                                    mapOf("Access-Control-Allow-Origin" to WEBUI_ORIGIN),
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
                                    mapOf("Access-Control-Allow-Origin" to WEBUI_ORIGIN),
                                    errorStream
                                )
                            }
                        }
                    }
                    if (!isTrustedWebUiUrl(url)) {
                        // The bridge can execute root commands, so do not allow any untrusted
                        // network resource or frame to be loaded into this WebView. Returning a
                        // response with a null body explicitly blocks the request; returning null
                        // would fall back to WebView's normal network loader.
                        return WebResourceResponse(null, null, null)
                    }
                    return webViewAssetLoader.shouldInterceptRequest(url)
                }

                override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                    val url = request.url
                    if (isTrustedWebUiUrl(url)) return false
                    if (request.isForMainFrame) openExternalUri(activity, url)
                    // Block subframes and unsupported schemes instead of forwarding arbitrary
                    // intent://, file://, content://, javascript:, or custom-scheme URLs.
                    return true
                }

                override fun onPageStarted(view: WebView, url: String, favicon: android.graphics.Bitmap?) {
                    val uri = Uri.parse(url)
                    if (!isTrustedWebUiUrl(uri)) {
                        // shouldOverrideUrlLoading is not guaranteed for every navigation (for
                        // example POST or some redirects). Stop those loads before untrusted HTML
                        // can execute in a WebView that has a root-capable JavaScript bridge.
                        view.stopLoading()
                        openExternalUri(activity, uri)
                        return
                    }
                    super.onPageStarted(view, url, favicon)
                }

                override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
                    webUIState.webCanGoBack = view?.canGoBack() ?: false
                    if (webUIState.isInsetsEnabled) webUIState.webView?.evaluateJavascript(webUIState.currentInsets.js, null)
                    super.doUpdateVisitedHistory(view, url, isReload)
                }
            }

            // WebChromeClient
            webView.webChromeClient = object : WebChromeClient() {
                override fun onJsAlert(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
                    if (message == null || result == null) return false
                    webUIState.uiEvent = WebUIEvent.ShowAlert(message, result)
                    return true
                }

                override fun onJsConfirm(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
                    if (message == null || result == null) return false
                    webUIState.uiEvent = WebUIEvent.ShowConfirm(message, result)
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
                    webUIState.uiEvent = WebUIEvent.ShowPrompt(message, defaultValue, result)
                    return true
                }

                override fun onShowFileChooser(
                    webView: WebView?, filePathCallback: ValueCallback<Array<Uri>>?, fileChooserParams: FileChooserParams?
                ): Boolean {
                    webUIState.filePathCallback?.onReceiveValue(null)
                    webUIState.filePathCallback = filePathCallback

                    val intent = fileChooserParams?.createIntent() ?: Intent(Intent.ACTION_GET_CONTENT).apply { type = "*/*" }
                    if (fileChooserParams?.mode == FileChooserParams.MODE_OPEN_MULTIPLE) {
                        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                    }
                    webUIState.uiEvent = WebUIEvent.ShowFileChooser(intent)
                    return true
                }
            }

            // JS Interface
            val webviewInterface = WebViewInterface(webUIState)
            webUIState.webView = webView
            webView.addJavascriptInterface(webviewInterface, "ksu")
            webUIState.uiEvent = WebUIEvent.WebViewReady
        }
    }
}