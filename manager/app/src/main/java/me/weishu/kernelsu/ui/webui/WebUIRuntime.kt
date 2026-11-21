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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong

/**
 * The WebUI host state that must outlive recomposition but cannot live in a ViewModel.
 *
 * A [WebView] instance is bound to the Activity that created it and leaks through a ViewModel, so
 * the view, the root shells it opened and the pending file-chooser callback are kept here and
 * scoped to the activity composition by `remember { WebUIRuntime() }`.
 */
@Stable
class WebUIRuntime {
    var webView: WebView? by mutableStateOf(null)
        private set

    private val fileChooserRequestId = AtomicLong(0L)
    private var pendingFileCallback: ValueCallback<Array<Uri>>? = null
    private var pendingFileChooserIntent: Intent? = null
    private val fileCallbackLock = Any()

    private val activeShells = ConcurrentHashMap.newKeySet<Shell>()
    private val shellLock = Any()
    @Volatile
    private var disposed = false
    private var viewInterface: WebViewInterface? = null
    private var downloadInterface: WebUIDownloadInterface? = null

    /** Owns the teardown work that must not block the main thread. Cancelled by [dispose]. */
    private val disposeScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    /**
     * Blob downloads for this window. The download interface registers them and the WebView
     * client serves the internal URLs, so both have to see the same instance.
     */
    val blobs = BlobDownloadHandler()

    fun attach(view: WebView) {
        disposeWebView()
        webView = view
    }

    /**
     * Returns false when the runtime is already gone. A caller that is resumed after the
     * composition was cancelled has nowhere to hand its shell, so it has to close it itself -
     * otherwise the shell outlives the window that opened it.
     */
    fun registerShell(shell: Shell): Boolean = synchronized(shellLock) {
        if (disposed) {
            false
        } else {
            activeShells.add(shell)
            true
        }
    }

    fun unregisterShell(shell: Shell) {
        activeShells.remove(shell)
    }

    fun registerInterfaces(webViewInterface: WebViewInterface, downloadInterface: WebUIDownloadInterface) {
        this.viewInterface = webViewInterface
        this.downloadInterface = downloadInterface
    }

    /**
     * Records the chooser the page is blocked on and returns its id, or null when another chooser
     * is still outstanding.
     *
     * The activity-result registry answers every launch of a key through one callback and carries
     * no launch identity, so a second chooser on screen would have its result delivered as if it
     * belonged to the first request. Refusing the newcomer keeps the outstanding chooser intact
     * instead of cancelling it from under the user.
     */
    fun beginFileChooser(intent: Intent, callback: ValueCallback<Array<Uri>>?): Long? =
        synchronized(fileCallbackLock) {
            if (pendingFileCallback != null) {
                null
            } else {
                pendingFileCallback = callback
                pendingFileChooserIntent = intent
                fileChooserRequestId.incrementAndGet()
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

    /**
     * Drops the current view without tearing the rest of the runtime down.
     *
     * Returning true from onRenderProcessGone is a promise that the app has already discarded
     * the WebView; touching it afterwards kills the process. So the view is detached here, while
     * the shells and the file-chooser callback are left for [dispose], which runs when the
     * composition goes away.
     */
    fun detachAndDestroy() {
        disposeWebView()
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
        val shellsToClose = synchronized(shellLock) {
            disposed = true
            val snapshot = activeShells.toSet()
            activeShells.clear()
            snapshot
        }
        synchronized(fileCallbackLock) {
            pendingFileCallback?.onReceiveValue(null)
            pendingFileCallback = null
            pendingFileChooserIntent = null
        }
        viewInterface?.destroy()
        viewInterface = null
        downloadInterface?.destroy()
        downloadInterface = null
        disposeWebView()
        if (shellsToClose.isNotEmpty()) {
            // Shell.close() blocks on the process it started, so it must not run on the main
            // thread - and a bare Thread is tied to nothing. A coroutine on the IO dispatcher
            // has the same threading and one owner; the scope is released once the work is
            // done, which is the earliest point it can be.
            disposeScope.launch {
                shellsToClose.forEach { shell -> runCatching { shell.close() } }
            }.invokeOnCompletion { disposeScope.cancel() }
        } else {
            disposeScope.cancel()
        }
    }
}
