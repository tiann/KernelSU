package me.weishu.kernelsu.ui.webui.model

import androidx.compose.runtime.Immutable

@Immutable
sealed interface WebUIBanner {
    val id: String

    data class OutdatedWebView(
        val currentVersion: Int,
        val requiredVersion: Int,
    ) : WebUIBanner {
        override val id: String get() = ID

        companion object {
            const val ID = "outdated_webview"
        }
    }
}
