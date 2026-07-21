package me.weishu.kernelsu.ui.webui.runtime

import android.net.Uri
import android.view.ViewGroup
import android.webkit.ValueCallback
import android.webkit.WebView
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.topjohnwu.superuser.Shell

class WebUIRuntime {
    var webView: WebView? by mutableStateOf(null)
    var rootShell: Shell? = null
    var pendingFileCallback: ValueCallback<Array<Uri>>? = null

    fun dispose() {
        webView?.let { view ->
            (view.parent as? ViewGroup)?.removeView(view)
            view.destroy()
        }
        webView = null
        rootShell?.close()
        rootShell = null
        pendingFileCallback?.onReceiveValue(null)
        pendingFileCallback = null
    }
}
