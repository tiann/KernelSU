package me.weishu.kernelsu.ui.webui.webview

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.webkit.WebView

@SuppressLint("SetJavaScriptEnabled")
internal fun createWebView(activity: Activity): WebView {
    val webView = WebView(activity)
    webView.setBackgroundColor(Color.TRANSPARENT)

    val prefs = activity.getSharedPreferences("settings", Context.MODE_PRIVATE)
    WebView.setWebContentsDebuggingEnabled(prefs.getBoolean("enable_web_debugging", false))

    webView.settings.apply {
        javaScriptEnabled = true
        domStorageEnabled = true
        allowFileAccess = false
    }

    return webView
}
