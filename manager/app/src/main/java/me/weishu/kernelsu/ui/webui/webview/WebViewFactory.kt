package me.weishu.kernelsu.ui.webui.webview

import android.annotation.SuppressLint
import android.app.Activity
import android.graphics.Color
import android.webkit.WebView
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl

@SuppressLint("SetJavaScriptEnabled")
internal fun createWebView(activity: Activity): WebView {
    val webView = WebView(activity)
    webView.setBackgroundColor(Color.TRANSPARENT)

    val repo = SettingsRepositoryImpl()
    WebView.setWebContentsDebuggingEnabled(repo.enableWebDebugging)

    webView.settings.apply {
        javaScriptEnabled = true
        domStorageEnabled = true
        allowFileAccess = false
    }

    return webView
}
