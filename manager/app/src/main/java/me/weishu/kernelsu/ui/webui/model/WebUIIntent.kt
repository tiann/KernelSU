package me.weishu.kernelsu.ui.webui.model

import android.content.Intent
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.WebView
import com.topjohnwu.superuser.Shell

sealed interface WebUIIntent {
    data class ModuleReady(val moduleName: String, val moduleDir: String, val shell: Shell, val webView: WebView) : WebUIIntent
    data class Error(val message: String) : WebUIIntent
    data object ExitRequested : WebUIIntent
    data object HomePageLoaded : WebUIIntent
    data class HistoryChanged(val canGoBack: Boolean) : WebUIIntent
    data class InsetsEnabledChanged(val enabled: Boolean) : WebUIIntent
    data class InsetsChanged(val insets: Insets) : WebUIIntent
    data class JsAlertRequested(val message: String, val result: JsResult) : WebUIIntent
    data class JsConfirmRequested(val message: String, val result: JsResult) : WebUIIntent
    data class JsPromptRequested(val message: String, val defaultValue: String, val result: JsPromptResult) : WebUIIntent
    data object AlertConfirmed : WebUIIntent
    data class ConfirmAnswered(val confirmed: Boolean) : WebUIIntent
    data class PromptAnswered(val value: String?) : WebUIIntent
    data class FileChooserRequested(val intent: Intent) : WebUIIntent
    data class FileChooserResult(val uris: Array<android.net.Uri>?) : WebUIIntent {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false

            other as FileChooserResult

            return uris.contentEquals(other.uris)
        }

        override fun hashCode(): Int {
            return uris?.contentHashCode() ?: 0
        }
    }
}
