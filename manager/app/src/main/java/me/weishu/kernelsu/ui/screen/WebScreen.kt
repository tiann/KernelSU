package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.Window
import android.webkit.JavascriptInterface
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.lifecycleScope
import com.google.accompanist.web.WebView
import com.google.accompanist.web.rememberWebViewState
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.topjohnwu.superuser.ShellUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.util.serveModule
import java.net.ServerSocket

@SuppressLint("SetJavaScriptEnabled")
@Destination
@Composable
fun WebScreen(navigator: DestinationsNavigator, moduleId: String, moduleName: String) {

    val port = 8080
    LaunchedEffect(Unit) {
        serveModule(moduleId, port)
    }

    val lifecycleOwner = LocalLifecycleOwner.current
    val context = LocalContext.current

    DisposableEffect(Unit) {
        onDispose {
            if (WebViewInterface.isHideSystemUI && context is Activity) {
                showSystemUI(context.window)
            }
            lifecycleOwner.lifecycleScope.launch {
                stopServer(port)
            }
        }
    }

    Scaffold { innerPadding ->
        WebView(
            state = rememberWebViewState(url = "http://localhost:$port"),
            Modifier
                .fillMaxSize()
                .padding(innerPadding),
            factory = { context ->
                android.webkit.WebView(context).apply {
                    settings.javaScriptEnabled = true
                    settings.domStorageEnabled = true
                    addJavascriptInterface(WebViewInterface(context), "ksu")
                }
            })
    }
}

class WebViewInterface(val context: Context) {

    companion object {
        var isHideSystemUI: Boolean = false
    }

    @JavascriptInterface
    fun exec(cmd: String): String {
        val shell = createRootShell()
        return ShellUtils.fastCmd(shell, cmd)
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

private suspend fun getFreePort(): Int {
    return withContext(Dispatchers.IO) {
        ServerSocket(0).use { socket -> socket.localPort }
    }
}

private suspend fun stopServer(port: Int) {
    withContext(Dispatchers.IO) {
        runCatching {
            okhttp3.OkHttpClient()
                .newCall(okhttp3.Request.Builder().url("http://localhost:$port/stop").build())
                .execute()
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