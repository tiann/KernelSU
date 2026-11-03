package me.weishu.kernelsu.ui.webui.ui.component

import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import me.weishu.kernelsu.ui.webui.webview.WEBUI_HOME_URL

@Composable
fun WebViewContainer(
    webView: WebView,
    onUrlLoaded: () -> Unit
) {
    val listener = object : View.OnLayoutChangeListener {
        override fun onLayoutChange(
            v: View, left: Int, top: Int, right: Int, bottom: Int,
            oldLeft: Int, oldTop: Int, oldRight: Int, oldBottom: Int,
        ) {
            if (v.width > 0 && v.height > 0) {
                v.removeOnLayoutChangeListener(this)
                (v as WebView).loadUrl(WEBUI_HOME_URL)
                onUrlLoaded()
            }
        }
    }
    AndroidView(
        modifier = Modifier.fillMaxSize(),
        factory = { _ ->
            (webView.parent as? ViewGroup)?.removeView(webView)
            webView.apply {
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
                if (width > 0 && height > 0) {
                    loadUrl(WEBUI_HOME_URL)
                    onUrlLoaded()
                } else {
                    addOnLayoutChangeListener(listener)
                }
            }
        },
        update = {},
    )
}
