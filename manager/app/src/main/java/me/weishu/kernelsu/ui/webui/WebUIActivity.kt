package me.weishu.kernelsu.ui.webui

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.os.Build
import android.os.Bundle
import android.view.ViewGroup.MarginLayoutParams
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.activity.ComponentActivity
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams
import androidx.lifecycle.lifecycleScope
import androidx.webkit.WebViewAssetLoader
import kotlinx.coroutines.launch
import com.topjohnwu.superuser.Shell
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import java.io.File

@SuppressLint("SetJavaScriptEnabled")
class WebUIActivity : ComponentActivity() {
    private lateinit var webviewInterface: WebViewInterface

    private var rootShell: Shell? = null
    private val superUserViewModel: SuperUserViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {

        // Enable edge to edge
        enableEdgeToEdge()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isNavigationBarContrastEnforced = false
        }

        super.onCreate(savedInstanceState)

        lifecycleScope.launch {
            superUserViewModel.fetchAppList()
        }

        val moduleId = intent.getStringExtra("id")!!
        val name = intent.getStringExtra("name")!!
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            @Suppress("DEPRECATION")
            setTaskDescription(ActivityManager.TaskDescription("KernelSU - $name"))
        } else {
            val taskDescription = ActivityManager.TaskDescription.Builder().setLabel("KernelSU - $name").build()
            setTaskDescription(taskDescription)
        }

        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
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
                val url = request.url

                // Handle ksu://icon/[packageName] to serve app icon via WebView
                if (url.scheme.equals("ksu", ignoreCase = true) && url.host.equals("icon", ignoreCase = true)) {
                    val packageName = url.path?.substring(1)
                    if (!packageName.isNullOrEmpty()) {
                        val icon = AppIconUtil.loadAppIconSync(this@WebUIActivity, packageName, 512)
                        if (icon != null) {
                            val stream = java.io.ByteArrayOutputStream()
                            icon.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, stream)
                            val inputStream = java.io.ByteArrayInputStream(stream.toByteArray())
                            return WebResourceResponse("image/png", null, inputStream)
                        }
                    }
                }

                return webViewAssetLoader.shouldInterceptRequest(url)
            }
        }

        val webView = WebView(this).apply {
            ViewCompat.setOnApplyWindowInsetsListener(this) { view, insets ->
                val inset = insets.getInsets(WindowInsetsCompat.Type.systemBars())
                view.updateLayoutParams<MarginLayoutParams> {
                    leftMargin = inset.left
                    rightMargin = inset.right
                    topMargin = inset.top
                    bottomMargin = inset.bottom
                }
                return@setOnApplyWindowInsetsListener insets
            }
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