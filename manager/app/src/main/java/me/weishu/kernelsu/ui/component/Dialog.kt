package me.weishu.kernelsu.ui.component

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import me.weishu.kernelsu.ui.util.LocalDialogHost
import kotlin.coroutines.resume

sealed interface DialogResult {
    object Confirmed : DialogResult
    object Dismissed : DialogResult
}

interface DialogVisuals {
    val title: String
    val content: String
    val confirm: String?
    val dismiss: String?
}

interface DialogData {
    val visuals: DialogVisuals

    fun confirm()

    fun dismiss()
}

class DialogHostState {

    private data class DialogVisualsImpl(
        override val title: String,
        override val content: String,
        override val confirm: String?,
        override val dismiss: String?
    ) : DialogVisuals

    private data class DialogDataImpl(
        override val visuals: DialogVisuals,
        val continuation: CancellableContinuation<DialogResult>
    ) : DialogData {

        override fun confirm() {
            if (continuation.isActive) continuation.resume(DialogResult.Confirmed)
        }

        override fun dismiss() {
            if (continuation.isActive) continuation.resume(DialogResult.Dismissed)
        }
    }

    private val mutex = Mutex()

    var currentDialogData by mutableStateOf<DialogData?>(null)
        private set

    suspend fun showDialog(
        title: String,
        content: String,
        confirm: String? = null,
        dismiss: String? = null
    ): DialogResult = mutex.withLock {
        try {
            return@withLock suspendCancellableCoroutine { continuation ->
                currentDialogData = DialogDataImpl(
                    visuals = DialogVisualsImpl(title, content, confirm, dismiss),
                    continuation = continuation
                )
            }
        } finally {
            currentDialogData = null
        }
    }
}

@Composable
fun rememberDialogHostState(): DialogHostState {
    return remember {
        DialogHostState()
    }
}

@Composable
fun BaseDialog(
    state: DialogHostState = LocalDialogHost.current,
    title: @Composable (String) -> Unit,
    confirmButton: @Composable (String?, () -> Unit) -> Unit,
    dismissButton: @Composable (String?, () -> Unit) -> Unit,
    content: @Composable (String) -> Unit = { Text(text = it) },
) {
    val currentDialogData = state.currentDialogData ?: return
    val visuals = currentDialogData.visuals
    AlertDialog(
        onDismissRequest = {
            currentDialogData.dismiss()
        },
        title = {
            title(visuals.title)
        },
        text = {
            content(visuals.content)
        },
        confirmButton = {
            confirmButton(visuals.confirm, currentDialogData::confirm)
        },
        dismissButton = {
            dismissButton(visuals.dismiss, currentDialogData::dismiss)
        }
    )
}

@Composable
fun SimpleDialog(
    state: DialogHostState = LocalDialogHost.current,
    content: @Composable (String) -> Unit
) {
    BaseDialog(
        state = state,
        title = {
            Text(text = it)
        },
        confirmButton = { text, confirm ->
            text?.let {
                TextButton(onClick = confirm) {
                    Text(text = it)
                }
            }
        },
        dismissButton = { text, dismiss ->
            text?.let {
                TextButton(onClick = dismiss) {
                    Text(text = it)
                }
            }
        },
        content = content
    )
}

@Composable
fun ConfirmDialog(state: DialogHostState = LocalDialogHost.current) {
    BaseDialog(
        state = state,
        title = {
            Text(text = it)
        },
        confirmButton = { text, confirm ->
            text?.let {
                TextButton(onClick = confirm) {
                    Text(text = it)
                }
            }
        },
        dismissButton = { text, dismiss ->
            text?.let {
                TextButton(onClick = dismiss) {
                    Text(text = it)
                }
            }
        },
    )
}