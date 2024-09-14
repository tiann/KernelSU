package me.weishu.kernelsu.ui.webui

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.content.Context
import android.os.Bundle
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.activity.ComponentActivity
import androidx.webkit.WebViewAssetLoader
import com.topjohnwu.superuser.Shell
import me.weishu.kernelsu.ui.util.createRootShell
import java.io.File

@SuppressLint("SetJavaScriptEnabled")
class WebUIActivity : ComponentActivity() {
    private lateinit var webviewInterface: WebViewInterface

    private var rootShell: Shell? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val moduleId = intent.getStringExtra("id")!!
        val name = intent.getStringExtra("name")!!
        setTaskDescription(ActivityManager.TaskDescription("KernelSU - $name"))

        val prefs = getSharedPreferences("settings", Context.MODE_PRIVATE)
        WebView.setWebContentsDebuggingEnabled(prefs.getBoolean("enable_web_debugging", false))

        val moduleDir = "/data/adb/modules/${moduleId}"
        val webRoot = File("${moduleDir}/webroot")
        val rootShell = createRootShell(true).also { this.rootShell = it }
        val webViewAssetLoader = WebViewAssetLoader.Builder()
            .setDomain("mui.kernelsu.org")
            .addPathHandler(
                "/",
                SuFilePathHandler(this, webRoot, rootShell)
            )
            .build()

        val webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(
                view: WebView,
                request: WebResourceRequest
            ): WebResourceResponse? {
                return webViewAssetLoader.shouldInterceptRequest(request.url)
            }
        }

        val webView = WebView(this).apply {
            settings.javaScriptEnabled = true
            settings.domStorageEnabled = true
            settings.allowFileAccess = false
            webviewInterface = WebViewInterface(this@WebUIActivity, this, moduleDir)
            addJavascriptInterface(webviewInterface, "ksu")
            setWebViewClient(webViewClient)
            loadUrl("https://mui.kernelsu.org/index.html")
        }

        setContentView(webView)
    }

    override fun onDestroy() {
        super.onDestroy()
        runCatching { rootShell?.close() }
    }
}