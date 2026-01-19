package me.weishu.kernelsu.ui.navigation3

import android.content.Intent
import android.net.Uri

/**
 * Deep link resolution: maps external Intent/Uri to an initial back stack.
 * Call resolve(intent) at Activity start to seed the back stack.
 */
object DeepLinkResolver {
    fun resolve(intent: Intent?): List<Route> {
        if (intent == null) return listOf(Route.Main)
        val shortcutType = intent.getStringExtra("shortcut_type")
        return when (shortcutType) {
            "module_action" -> {
                val moduleId = intent.getStringExtra("module_id") ?: return listOf(Route.Main)
                listOf(Route.Main, Route.ExecuteModuleAction(moduleId))
            }

            else -> listOf(Route.Main)
        }
    }

    fun resolve(uri: Uri?): List<Route> {
        return listOf(Route.Main)
    }
}
