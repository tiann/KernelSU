package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.text.TextUtils
import android.view.Window
import android.webkit.JavascriptInterface
import android.webkit.WebView
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.accompanist.web.WebView
import com.google.accompanist.web.rememberWebViewState
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.topjohnwu.superuser.ShellUtils
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.util.serveModule
import org.json.JSONObject

@SuppressLint("SetJavaScriptEnabled")
@Destination
@Composable
fun WebScreen(navigator: DestinationsNavigator, moduleId: String, moduleName: String) {

    LaunchedEffect(Unit) {
        serveModule(moduleId)
    }

    val context = LocalContext.current

    DisposableEffect(Unit) {
        onDispose {
            if (WebViewInterface.isHideSystemUI && context is Activity) {
                showSystemUI(context.window)
            }
        }
    }

    Scaffold { innerPadding ->
        WebView(
            state = rememberWebViewState(url = "file:///data/data/me.weishu.kernelsu/webroot/index.html"),
            Modifier
                .fillMaxSize()
                .padding(innerPadding),
            factory = { context ->
                WebView(context).apply {
                    settings.javaScriptEnabled = true
                    settings.domStorageEnabled = true
                    settings.allowFileAccess = true
                    addJavascriptInterface(WebViewInterface(context, this), "ksu")
                }
            })
    }
}

class WebViewInterface(val context: Context, val webView: WebView) {

    companion object {
        var isHideSystemUI: Boolean = false
    }

    @JavascriptInterface
    fun exec(cmd: String): String {
        val shell = createRootShell()
        return ShellUtils.fastCmd(shell, cmd)
    }

    @JavascriptInterface
    fun exec(cmd: String, successCallbackName: String, errorCallbackName: String) {
        exec(cmd, null, successCallbackName, errorCallbackName)
    }

    @JavascriptInterface
    fun exec(
        cmd: String,
        options: String?,
        successCallbackName: String,
        errorCallbackName: String
    ) {
        val opts = if (options == null) JSONObject() else {
            JSONObject(options)
        }

        val finalCommand = StringBuilder()

        val cwd = opts.optString("cwd")
        if (!TextUtils.isEmpty(cwd)) {
            finalCommand.append("cd ${cwd};")
        }

        opts.optJSONObject("env")?.let { env ->
            env.keys().forEach { key ->
                finalCommand.append("export ${key}=${env.getString(key)};")
            }
        }

        finalCommand.append(cmd)

        val shell = createRootShell()
        val result = shell.newJob().add(finalCommand.toString()).to(ArrayList(), ArrayList()).exec()
        if (!result.isSuccess) {
            val jsCode =
                "javascript: (function() { try { ${errorCallbackName}(${result.code}); } catch(e) { console.error(e); } })();"
            webView.post {
                webView.loadUrl(jsCode)
            }
        } else {
            val stdout = result.out.joinToString(separator = "\n")
            val stderr = result.err.joinToString(separator = "\n")

            val jsCode =
                "javascript: (function() { try { ${successCallbackName}(${JSONObject.quote(stdout)}, ${
                    JSONObject.quote(
                        stderr
                    )
                }); } catch(e) { console.error(e); } })();"
            webView.post {
                webView.loadUrl(jsCode)
            }
        }
    }

    @JavascriptInterface
    fun fullScreen(enable: Boolean) {
        if (context is Activity) {
            Handler(Looper.getMainLooper()).post {
                if (enable) {
                    hideSystemUI(context.window)
                } else {
                    showSystemUI(context.window)
                }
                isHideSystemUI = enable
            }
        }
    }

}

private fun hideSystemUI(window: Window) {
    WindowCompat.setDecorFitsSystemWindows(window, false)
    WindowInsetsControllerCompat(window, window.decorView).let { controller ->
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
}

private fun showSystemUI(window: Window) {
    WindowCompat.setDecorFitsSystemWindows(window, true)
    WindowInsetsControllerCompat(
        window,
        window.decorView
    ).show(WindowInsetsCompat.Type.systemBars())
}