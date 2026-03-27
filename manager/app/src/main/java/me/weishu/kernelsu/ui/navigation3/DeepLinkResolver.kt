package me.weishu.kernelsu.ui.navigation3

import android.app.Activity
import android.content.Intent
import android.net.Uri
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.core.net.toUri
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.util.DownloadService

/**
 * Deep link resolution: maps external Intent/Uri to an initial back stack.
 * Call resolve(intent) at Activity start to seed the back stack.
 */
object DeepLinkResolver {
    fun resolve(intent: Intent?): List<Route> {
        if (intent == null) return emptyList()
        if (intent.action == DownloadService.ACTION_INSTALL_MODULE) {
            val uriString = intent.getStringExtra(DownloadService.EXTRA_MODULE_URI)
                ?: return emptyList()
            val uri = uriString.toUri()
            return listOf(Route.Main, Route.Flash(FlashIt.FlashModules(listOf(uri))))
        }

        val shortcutType = intent.getStringExtra("shortcut_type")
        return when (shortcutType) {
            "module_action" -> {
                val moduleId = intent.getStringExtra("module_id") ?: return emptyList()
                listOf(Route.Main, Route.ExecuteModuleAction(moduleId, fromShortcut = true))
            }

            else -> emptyList()
        }
    }

    fun resolve(uri: Uri?): List<Route> {
        return emptyList()
    }
}

/**
 * Composable that handles deep link intents and updates the back stack accordingly.
 * Should be placed at the root of the NavHost.
 */
@Composable
fun HandleDeepLink(
    intentState: State<Int>,
) {
    val context = LocalContext.current
    val activity = context as? Activity
    val currentIntentId by intentState
    val navigator = LocalNavigator.current
    var lastHandledIntentId by rememberSaveable { mutableIntStateOf(-1) }

    LaunchedEffect(currentIntentId) {
        if (currentIntentId != lastHandledIntentId) {
            val intent = activity?.intent
            val initialStack = DeepLinkResolver.resolve(intent)
            if (initialStack.isNotEmpty()) {
                navigator.replaceAll(initialStack)
                intent?.removeExtra("shortcut_type")
                intent?.removeExtra("module_id")
                if (intent?.action == DownloadService.ACTION_INSTALL_MODULE) {
                    intent.action = null
                    intent.removeExtra(DownloadService.EXTRA_MODULE_URI)
                    intent.removeExtra(DownloadService.EXTRA_DOWNLOAD_ID)
                }
            }
            lastHandledIntentId = currentIntentId
        }
    }
}
