package me.weishu.kernelsu.ui.webui.ui

import android.app.Activity
import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue

/**
 * Hoisted on purpose: `rememberLauncherForActivityResult` keys its registration effect on the
 * contract instance, and [ActivityResultContracts.StartActivityForResult] has no equals, so a new
 * instance per composition would unregister and re-register the launcher - unregistering drops a
 * result the registry is still holding.
 */
private val START_ACTIVITY_FOR_RESULT = ActivityResultContracts.StartActivityForResult()

@Composable
fun rememberFileLauncher(onResult: (Long, Array<Uri>?) -> Unit): (Long, Intent) -> Unit {
    var pendingRequestId by rememberSaveable { mutableStateOf<Long?>(null) }
    val currentOnResult by rememberUpdatedState(onResult)
    val launcher = rememberLauncherForActivityResult(
        contract = START_ACTIVITY_FOR_RESULT
    ) { result ->
        val requestId = pendingRequestId
        if (requestId != null) {
            val uris: Array<Uri>? = if (result.resultCode == Activity.RESULT_OK) {
                result.data?.let { data ->
                    data.clipData?.let { clipData ->
                        Array(clipData.itemCount) { i -> clipData.getItemAt(i).uri }
                    } ?: data.data?.let { arrayOf(it) }
                }
            } else null
            currentOnResult(requestId, uris)
            pendingRequestId = null
        }
    }

    return remember(launcher) {
        { requestId, intent ->
            if (pendingRequestId != null) {
                // The registry answers every launch of a key through one callback and carries no
                // launch identity, so a second chooser on screen would have its result delivered
                // as if it belonged to this request. Answer the page with "cancelled" instead.
                currentOnResult(requestId, null)
            } else {
                pendingRequestId = requestId
                try {
                    launcher.launch(intent)
                } catch (_: Exception) {
                    currentOnResult(requestId, null)
                    pendingRequestId = null
                }
            }
        }
    }
}
