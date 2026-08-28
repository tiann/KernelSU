package me.weishu.kernelsu.ui.webui.ui

import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.runtime.WebUIRuntime
import me.weishu.kernelsu.ui.webui.webview.WEBUI_HOME_URL

@Composable
internal fun WebViewContainer(
    state: WebUIState,
    runtime: WebUIRuntime,
    dispatch: (WebUIIntent) -> Unit,
) {
    runtime.webView?.let { webView ->
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { _ ->
                webView.apply {
                    layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT
                    )
                    if (!state.isUrlLoaded) {
                        if (width > 0 && height > 0) {
                            loadUrl(WEBUI_HOME_URL)
                            dispatch(WebUIIntent.HomePageLoaded)
                        } else {
                            val listener = object : View.OnLayoutChangeListener {
                                override fun onLayoutChange(
                                    v: View, left: Int, top: Int, right: Int, bottom: Int,
                                    oldLeft: Int, oldTop: Int, oldRight: Int, oldBottom: Int
                                ) {
                                    if (v.width > 0 && v.height > 0) {
                                        (v as WebView).loadUrl(WEBUI_HOME_URL)
                                        dispatch(WebUIIntent.HomePageLoaded)
                                        v.removeOnLayoutChangeListener(this)
                                    }
                                }
                            }
                            addOnLayoutChangeListener(listener)
                        }
                    }
                }
            },
            update = { view ->
                view.requestLayout()
            }
        )
    }
}
