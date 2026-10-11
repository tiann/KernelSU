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

@Composable
fun rememberFileLauncher(onResult: (Long, Array<Uri>?) -> Unit): (Long, Intent) -> Unit {
    var pendingRequestId by rememberSaveable { mutableStateOf<Long?>(null) }
    val currentOnResult by rememberUpdatedState(onResult)
    val launcher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
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
