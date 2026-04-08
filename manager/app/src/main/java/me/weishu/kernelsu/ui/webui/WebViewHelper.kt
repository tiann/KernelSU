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
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import java.io.File

private const val DOWNLOAD_JS = """
    (function() {
        if (window.ksu_download_enabled) return;
        window.ksu_download_enabled = true;
        const blobMap = new Map();
        const originalCreateObjectURL = URL.createObjectURL;
        URL.createObjectURL = (obj) => {
            const url = originalCreateObjectURL(obj);
            if (obj instanceof Blob) blobMap.set(url, obj);
            return url;
        };
        const originalRevokeObjectURL = URL.revokeObjectURL;
        URL.revokeObjectURL = (url) => {
            setTimeout(() => blobMap.delete(url), 10000);
            return originalRevokeObjectURL(url);
        };
        const handleDownload = async (anchor) => {
            const url = new URL(anchor.href, location.href);
            const fileName = anchor.download || url.pathname.split("/").pop().split("?")[0] || "download.bin";
            const isInternal = url.hostname === 'mui.kernelsu.org';
            if (url.protocol === 'blob:' || url.protocol === 'data:' || isInternal) {
                const blob = (url.protocol === 'blob:' && blobMap.has(url.href)) ? blobMap.get(url.href) : await (await fetch(url.href, { credentials: 'include' })).blob();
                const base64 = await new Promise((resolve, reject) => {
                    const reader = new FileReader();
                    reader.onload = () => resolve(reader.result.split(',')[1] || '');
                    reader.onerror = () => reject(reader.error || new Error('Failed to read blob'));
                    reader.readAsDataURL(blob);
                });
                ksu_download.save(base64, fileName);
                return;
            }
            ksu_download.download(url.href, fileName, anchor.type || null);
        };
        document.addEventListener('click', (event) => {
            const anchor = event.target.closest('a[download]');
            if (!anchor || !anchor.href) return;
            event.preventDefault();
            handleDownload(anchor).catch((error) => console.error('KernelSU download failed', error));
        }, true);
        const originalClick = HTMLAnchorElement.prototype.click;
        HTMLAnchorElement.prototype.click = function() {
            if (this.hasAttribute('download') && this.href) {
                handleDownload(this).catch((error) => console.error('KernelSU download failed', error));
                return;
            }
            return originalClick.apply(this, arguments);
        };
    })();
"""

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
                allowFileAccess = false
            }

            val webRoot = File("${webUIState.modDir}/webroot")
            val webViewAssetLoader = WebViewAssetLoader.Builder()
                .setDomain("mui.kernelsu.org")
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
                            val icon = AppIconUtil.loadAppIconSync(activity, packageName, 512)
                            if (icon != null) {
                                val stream = java.io.ByteArrayOutputStream()
                                icon.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, stream)
                                return WebResourceResponse(
                                    "image/png", null, 200, "OK",
                                    mapOf("Access-Control-Allow-Origin" to "*"),
                                    java.io.ByteArrayInputStream(stream.toByteArray())
                                )
                            }
                        }
                    }
                    return webViewAssetLoader.shouldInterceptRequest(url)
                }

                override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
                    webUIState.webCanGoBack = view?.canGoBack() ?: false
                    if (webUIState.isInsetsEnabled) webUIState.webView?.evaluateJavascript(webUIState.currentInsets.js, null)
                    view?.evaluateJavascript(DOWNLOAD_JS, null)
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
            val downloadInterface = WebUIDownloadInterface(webUIState)
            webUIState.webViewInterface = webviewInterface
            webUIState.downloadInterface = downloadInterface
            webUIState.webView = webView
            webView.addJavascriptInterface(webviewInterface, "ksu")
            webView.addJavascriptInterface(downloadInterface, "ksu_download")
            webView.setDownloadListener { url, _, contentDisposition, mimetype, _ ->
                val fileName = android.webkit.URLUtil.guessFileName(url, contentDisposition, mimetype)
                downloadInterface.download(url, fileName, mimetype)
            }
            webView.evaluateJavascript(DOWNLOAD_JS, null)
            webUIState.uiEvent = WebUIEvent.WebViewReady
        }
    }
}
