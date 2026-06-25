package me.weishu.kernelsu.ui.webui.ui

import android.app.Activity
import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIState

@Composable
internal fun rememberFileLauncher(dispatch: (WebUIIntent) -> Unit): ActivityResultLauncher<Intent> {
    return rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { result ->
        val uris: Array<Uri>? = if (result.resultCode == Activity.RESULT_OK) {
            result.data?.let { data ->
                data.clipData?.let { clipData ->
                    Array(clipData.itemCount) { i -> clipData.getItemAt(i).uri }
                } ?: data.data?.let { arrayOf(it) }
            }
        } else null
        dispatch(WebUIIntent.FileChooserResult(uris))
    }
}

@Composable
internal fun HandleWebUIEvent(
    state: WebUIState,
    dispatch: (WebUIIntent) -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> HandleWebUIEventMiuix(state, dispatch)
        UiMode.Material -> HandleWebUIEventMaterial(state, dispatch)
    }
}
