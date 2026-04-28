package me.weishu.kernelsu.ui.util

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import me.weishu.kernelsu.R

fun openExternalUrl(context: Context, url: String) {
    if (url.isBlank()) return
    runCatching {
        context.startActivity(
            Intent(Intent.ACTION_VIEW, Uri.parse(url)).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
        )
    }.onFailure {
        val messageRes = when (it) {
            is SecurityException,
            is ActivityNotFoundException -> R.string.wear_open_link_unavailable

            else -> R.string.wear_open_link_failed
        }
        Toast.makeText(context, messageRes, Toast.LENGTH_SHORT).show()
    }
}
