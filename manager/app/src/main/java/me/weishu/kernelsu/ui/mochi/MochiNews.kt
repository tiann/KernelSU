package me.weishu.kernelsu.ui.mochi

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.core.content.edit

/**
 * Shows a dot on the settings tab and the UI style row until the user picks a UI style.
 */
object MochiNews {
    private const val KEY = "mochi_ui_news_seen"

    private var seen by mutableStateOf<Boolean?>(null)

    fun unseen(context: Context): Boolean {
        val value = seen ?: prefs(context).getBoolean(KEY, false).also { seen = it }
        return !value
    }

    fun markSeen(context: Context) {
        if (seen == true) return
        seen = true
        prefs(context).edit { putBoolean(KEY, true) }
    }

    private fun prefs(context: Context) =
        context.applicationContext.getSharedPreferences("settings", Context.MODE_PRIVATE)
}

private val DotRed = Color(0xFFFF3B30)

@Composable
fun NewUiDot(modifier: Modifier = Modifier) {
    Box(modifier.size(8.dp).background(DotRed, CircleShape))
}
