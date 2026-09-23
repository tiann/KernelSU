package me.weishu.kernelsu.ui.webui

import android.content.Intent
import android.net.Uri
import android.view.ViewGroup
import android.webkit.ValueCallback
import android.webkit.WebView
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.topjohnwu.superuser.Shell
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong

@Stable
class WebUIRuntime {
    var webView: WebView? by mutableStateOf(null)
        private set

    private val fileChooserRequestId = AtomicLong(0L)
    private var pendingFileCallback: ValueCallback<Array<Uri>>? = null
    private var pendingFileChooserIntent: Intent? = null
    private val fileCallbackLock = Any()

    private val activeShells = ConcurrentHashMap.newKeySet<Shell>()

    fun attach(view: WebView) {
        disposeWebView()
        webView = view
    }

    fun registerShell(shell: Shell) {
        activeShells.add(shell)
    }

    fun unregisterShell(shell: Shell) {
        activeShells.remove(shell)
    }

    fun beginFileChooser(intent: Intent, callback: ValueCallback<Array<Uri>>?): Long {
        synchronized(fileCallbackLock) {
            pendingFileCallback?.onReceiveValue(null)
            pendingFileCallback = callback
            pendingFileChooserIntent = intent
            return fileChooserRequestId.incrementAndGet()
        }
    }

    fun takeFileChooserIntent(requestId: Long): Intent? {
        synchronized(fileCallbackLock) {
            return if (requestId == fileChooserRequestId.get()) pendingFileChooserIntent else null
        }
    }

    fun consumePendingFileCallback(requestId: Long, uris: Array<Uri>?) {
        synchronized(fileCallbackLock) {
            if (requestId == fileChooserRequestId.get()) {
                pendingFileCallback?.onReceiveValue(uris)
                pendingFileCallback = null
                pendingFileChooserIntent = null
            }
        }
    }

    private fun disposeWebView() {
        webView?.let { view ->
            view.stopLoading()
            (view.parent as? ViewGroup)?.removeView(view)
            view.destroy()
        }
        webView = null
    }

    fun dispose() {
        synchronized(fileCallbackLock) {
            pendingFileCallback?.onReceiveValue(null)
            pendingFileCallback = null
            pendingFileChooserIntent = null
        }
        disposeWebView()
        val shellsToClose = activeShells.toSet()
        activeShells.clear()
        if (shellsToClose.isNotEmpty()) {
            Thread {
                shellsToClose.forEach { shell ->
                    runCatching { shell.close() }
                }
            }.start()
        }
    }
}
