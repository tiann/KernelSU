package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.app.Activity
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.webkit.WebViewAssetLoader
import com.google.accompanist.web.AccompanistWebViewClient
import com.google.accompanist.web.WebView
import com.google.accompanist.web.rememberWebViewState
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import me.weishu.kernelsu.ui.webui.SuFilePathHandler
import me.weishu.kernelsu.ui.webui.WebViewInterface
import me.weishu.kernelsu.ui.webui.showSystemUI
import java.io.File

@SuppressLint("SetJavaScriptEnabled")
@Destination
@Composable
fun WebScreen(navigator: DestinationsNavigator, moduleId: String, moduleName: String) {

    val context = LocalContext.current

    DisposableEffect(Unit) {
        onDispose {
            if (WebViewInterface.isHideSystemUI && context is Activity) {
                showSystemUI(context.window)
            }
        }
    }

    Scaffold { innerPadding ->
        val webRoot = File("/data/adb/modules/${moduleId}/webroot")
        val webViewAssetLoader = WebViewAssetLoader.Builder()
            .setDomain("mui.kernelsu.org")
            .addPathHandler("/",
                SuFilePathHandler(context, webRoot)
            )
            .build()

        val webViewClient = object : AccompanistWebViewClient() {
            override fun shouldInterceptRequest(
                view: WebView,
                request: WebResourceRequest
            ): WebResourceResponse? {
                return webViewAssetLoader.shouldInterceptRequest(request.url)
            }
        }
        WebView(
            state = rememberWebViewState(url = "https://mui.kernelsu.org/index.html"),
            Modifier
                .fillMaxSize()
                .padding(innerPadding),
            client = webViewClient,
            factory = { context ->
                WebView(context).apply {
                    settings.javaScriptEnabled = true
                    settings.domStorageEnabled = true
                    settings.allowFileAccess = false
                    addJavascriptInterface(WebViewInterface(context, this), "ksu")
                }
            })
    }
}