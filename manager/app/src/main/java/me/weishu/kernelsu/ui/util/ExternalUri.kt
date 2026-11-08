package me.weishu.kernelsu.ui.util

import android.content.Context
import android.widget.Toast
import androidx.compose.ui.platform.UriHandler
import me.weishu.kernelsu.R

fun openUriSafely(context: Context, uriHandler: UriHandler, uri: String) {
    try {
        uriHandler.openUri(uri)
    } catch (e: IllegalArgumentException) {
        Toast.makeText(context, R.string.external_link_open_failed, Toast.LENGTH_SHORT).show()
    }
}
