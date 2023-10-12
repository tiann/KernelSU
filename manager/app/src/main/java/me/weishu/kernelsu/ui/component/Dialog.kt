package me.weishu.kernelsu.ui.component

import android.graphics.text.LineBreaker
import android.text.Layout
import android.text.method.LinkMovementMethod
import android.view.ViewGroup
import android.widget.TextView
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import io.noties.markwon.Markwon
import io.noties.markwon.utils.NoCopySpannableFactory
import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import me.weishu.kernelsu.ui.util.LocalDialogHost
import kotlin.coroutines.resume

interface DialogVisuals

interface LoadingDialogVisuals : DialogVisuals

interface PromptDialogVisuals : DialogVisuals {
    val title: String
    val content: String
}

interface ConfirmDialogVisuals : PromptDialogVisuals {
    val confirm: String?
    val dismiss: String?
    val isMarkdown: Boolean
}


sealed interface DialogData {
    val visuals: DialogVisuals
}

interface LoadingDialogData : DialogData {
    override val visuals: LoadingDialogVisuals
    fun dismiss()
}

interface PromptDialogData : DialogData {
    override val visuals: PromptDialogVisuals
    fun dismiss()
}

interface ConfirmDialogData : PromptDialogData {
    override val visuals: ConfirmDialogVisuals
    fun confirm()
}

sealed interface ConfirmResult {
    object Confirmed : ConfirmResult
    object Canceled : ConfirmResult
}

class DialogHostState {

    private object LoadingDialogVisualsImpl : LoadingDialogVisuals

    private data class PromptDialogVisualsImpl(
        override val title: String, override val content: String
    ) : PromptDialogVisuals

    private data class ConfirmDialogVisualsImpl(
        override val title: String,
        override val content: String,
        override val confirm: String?,
        override val dismiss: String?,
        override val isMarkdown: Boolean,
    ) : ConfirmDialogVisuals

    private data class LoadingDialogDataImpl(
        override val visuals: LoadingDialogVisuals,
        private val continuation: CancellableContinuation<Unit>,
    ) : LoadingDialogData {
        override fun dismiss() {
            if (continuation.isActive) continuation.resume(Unit)
        }
    }

    private data class PromptDialogDataImpl(
        override val visuals: PromptDialogVisuals,
        private val continuation: CancellableContinuation<Unit>,
    ) : PromptDialogData {
        override fun dismiss() {
            if (continuation.isActive) continuation.resume(Unit)
        }
    }

    private data class ConfirmDialogDataImpl(
        override val visuals: ConfirmDialogVisuals,
        private val continuation: CancellableContinuation<ConfirmResult>
    ) : ConfirmDialogData {

        override fun confirm() {
            if (continuation.isActive) continuation.resume(ConfirmResult.Confirmed)
        }

        override fun dismiss() {
            if (continuation.isActive) continuation.resume(ConfirmResult.Canceled)
        }
    }

    private val mutex = Mutex()

    var currentDialogData by mutableStateOf<DialogData?>(null)
        private set

    suspend fun showLoading() {
        try {
            mutex.withLock {
                suspendCancellableCoroutine { continuation ->
                    currentDialogData = LoadingDialogDataImpl(
                        visuals = LoadingDialogVisualsImpl, continuation = continuation
                    )
                }
            }
        } finally {
            currentDialogData = null
        }
    }

    suspend fun <R> withLoading(block: suspend () -> R) = coroutineScope {
        val showLoading = launch {
            showLoading()
        }

        val result = block()

        showLoading.cancel()

        result
    }

    suspend fun showPrompt(title: String, content: String) {
        try {
            mutex.withLock {
                suspendCancellableCoroutine { continuation ->
                    currentDialogData = PromptDialogDataImpl(
                        visuals = PromptDialogVisualsImpl(title, content),
                        continuation = continuation
                    )
                }
            }
        } finally {
            currentDialogData = null
        }
    }

    suspend fun showConfirm(
        title: String,
        content: String,
        markdown: Boolean = false,
        confirm: String? = null,
        dismiss: String? = null
    ): ConfirmResult = mutex.withLock {
        try {
            return@withLock suspendCancellableCoroutine { continuation ->
                currentDialogData = ConfirmDialogDataImpl(
                    visuals = ConfirmDialogVisualsImpl(title, content, confirm, dismiss, markdown),
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

private inline fun <reified T : DialogData> DialogData?.tryInto(): T? {
    return when (this) {
        is T -> this
        else -> null
    }
}

@Composable
fun LoadingDialog(
    state: DialogHostState = LocalDialogHost.current,
) {
    state.currentDialogData.tryInto<LoadingDialogData>() ?: return
    val dialogProperties = remember {
        DialogProperties(dismissOnClickOutside = false, dismissOnBackPress = false)
    }
    Dialog(onDismissRequest = {}, properties = dialogProperties) {
        Surface(
            modifier = Modifier.size(100.dp), shape = RoundedCornerShape(8.dp)
        ) {
            Box(
                contentAlignment = Alignment.Center,
            ) {
                CircularProgressIndicator()
            }
        }
    }
}

@Composable
fun PromptDialog(
    state: DialogHostState = LocalDialogHost.current,
) {
    val promptDialogData = state.currentDialogData.tryInto<PromptDialogData>() ?: return

    val visuals = promptDialogData.visuals
    AlertDialog(
        onDismissRequest = {
            promptDialogData.dismiss()
        },
        title = {
            Text(text = visuals.title)
        },
        text = {
            Text(text = visuals.content)
        },
        confirmButton = {
            TextButton(onClick = { promptDialogData.dismiss() }) {
                Text(text = stringResource(id = android.R.string.ok))
            }
        },
        dismissButton = null,
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfirmDialog(state: DialogHostState = LocalDialogHost.current) {
    val confirmDialogData = state.currentDialogData.tryInto<ConfirmDialogData>() ?: return

    val visuals = confirmDialogData.visuals

    AlertDialog(
        onDismissRequest = {
            confirmDialogData.dismiss()
        },
        title = {
            Text(text = visuals.title)
        },
        text = {
            if (visuals.isMarkdown) {
                MarkdownContent(content = visuals.content)
            } else {
                Text(text = visuals.content)
            }
        },
        confirmButton = {
            TextButton(onClick = { confirmDialogData.confirm() }) {
                Text(text = visuals.confirm ?: stringResource(id = android.R.string.ok))
            }
        },
        dismissButton = {
            TextButton(onClick = { confirmDialogData.dismiss() }) {
                Text(text = visuals.dismiss ?: stringResource(id = android.R.string.cancel))
            }
        },
    )
}
@Composable
private fun MarkdownContent(content: String) {
    val contentColor = LocalContentColor.current

    AndroidView(
        factory = { context ->
            TextView(context).apply {
                movementMethod = LinkMovementMethod.getInstance()
                setSpannableFactory(NoCopySpannableFactory.getInstance())
                breakStrategy = LineBreaker.BREAK_STRATEGY_SIMPLE
                hyphenationFrequency = Layout.HYPHENATION_FREQUENCY_NONE
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                )
            }
        },
        modifier = Modifier
            .fillMaxWidth()
            .wrapContentHeight(),
        update = {
            Markwon.create(it.context).setMarkdown(it, content)
            it.setTextColor(contentColor.toArgb())
        })
}