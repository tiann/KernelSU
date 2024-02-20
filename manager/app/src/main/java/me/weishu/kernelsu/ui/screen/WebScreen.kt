package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.webkit.JavascriptInterface
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLifecycleOwner
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

    DisposableEffect(Unit) {
        onDispose {
            lifecycleOwner.lifecycleScope.launch {
                stopServer(port)
            }
        }
    }

    Scaffold(topBar = {
        TopBar(moduleName)
    }) { innerPadding ->
        WebView(
            state = rememberWebViewState(url = "http://localhost:$port"),
            Modifier
                .fillMaxSize()
                .padding(innerPadding),
            factory = { context ->
                android.webkit.WebView(context).apply {
                    settings.javaScriptEnabled = true
                    settings.domStorageEnabled = true
                    addJavascriptInterface(WebViewInterface(), "ksu")
                }
            })
    }
}

class WebViewInterface {
    @JavascriptInterface
    fun exec(cmd: String): String {
        val shell = createRootShell()
        return ShellUtils.fastCmd(shell, cmd)
    }

}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(title: String) {
    TopAppBar(title = { Text(title) })
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