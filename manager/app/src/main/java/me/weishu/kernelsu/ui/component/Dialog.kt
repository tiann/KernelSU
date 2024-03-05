package me.weishu.kernelsu.ui.component

import android.graphics.text.LineBreaker
import android.os.Parcelable
import android.text.Layout
import android.text.method.LinkMovementMethod
import android.util.Log
import android.view.ViewGroup
import android.widget.TextView
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.Saver
import androidx.compose.runtime.saveable.rememberSaveable
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
import kotlinx.coroutines.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.ui.util.LocalDialogHost
import kotlin.coroutines.resume

private const val TAG = "DialogComponent"

interface DialogVisuals

interface LoadingDialogVisuals : DialogVisuals

interface CustomDialogVisuals : DialogVisuals

interface ConfirmDialogVisuals : DialogVisuals, Parcelable {
    val title: String
    val content: String
    val isMarkdown: Boolean
    val confirm: String?
    val dismiss: String?
}


sealed interface DialogData {
    val visuals: DialogVisuals
    fun dismiss()

    fun dialogType() = when (this) {
        is LoadingDialogData -> "LoadingDialog"
        is ConfirmDialogData -> "ConfirmDialog"
        is CustomDialogData -> "CustomDialog"
    }
}

interface LoadingDialogData : DialogData {
    override val visuals: LoadingDialogVisuals
}

interface CustomDialogData : DialogData {
    override val visuals: CustomDialogVisuals
    val composable: @Composable (() -> Unit) -> Unit
}

interface ConfirmDialogData : DialogData {
    override val visuals: ConfirmDialogVisuals
    fun confirm()
}

sealed interface ConfirmResult {
    object Confirmed : ConfirmResult
    object Canceled : ConfirmResult
}

interface DialogHost {
    val dialogDataList: List<DialogData>
    suspend fun showLoading()
    suspend fun <R> withLoading(block: suspend () -> R): R
    suspend fun showConfirm(visuals: ConfirmDialogVisuals): ConfirmResult
    suspend fun showCustom(composable: @Composable (dismiss: () -> Unit) -> Unit)
}

class DialogHostState : DialogHost {
    @Parcelize
    private data class ConfirmDialogVisualsImpl(
        override val title: String,
        override val content: String,
        override val isMarkdown: Boolean,
        override val confirm: String?,
        override val dismiss: String?,
    ) : ConfirmDialogVisuals

    private object LoadingDialogVisualsImpl : LoadingDialogVisuals

    private object CustomDialogVisualsImpl : CustomDialogVisuals

    private data class LoadingDialogDataImpl(
        private val continuation: CancellableContinuation<Unit>,
    ) : LoadingDialogData {
        override val visuals: LoadingDialogVisuals = LoadingDialogVisualsImpl

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

    private data class CustomDialogDataImpl(
        override val composable: @Composable (() -> Unit) -> Unit,
        private val continuation: CancellableContinuation<Unit>
    ) : CustomDialogData {
        override val visuals: CustomDialogVisuals = CustomDialogVisualsImpl
        override fun dismiss() {
            if (continuation.isActive) continuation.resume(Unit)
        }
    }

    private val mutex = Mutex()

    private val shownDataList = mutableStateListOf<DialogData>()

    private suspend fun pushDialogData(dialogData: DialogData) {
        mutex.withLock {
            Log.d(TAG, "${dialogData.dialogType()} will be shown")
            shownDataList.add(dialogData)
        }
    }

    private suspend fun popDialogData() {
        mutex.withLock {
            shownDataList.removeLastOrNull()?.also {
                it.dismiss()
                Log.d(TAG, "${it.dialogType()} will be hidden")
            }
        }
    }

    private suspend fun throwIfLoadingShown() {
        mutex.withLock {
            if (shownDataList.lastOrNull() is LoadingDialogData) {
                Log.e(TAG, "Someone attempts to show dialog while loading dialog has been shown")
                throw CancellationException("show dialog canceled. can't show dialog while loading dialog has been shown")
            }
        }
    }

    private suspend fun <R> withCoroutineScope(block: suspend CoroutineScope.() -> R): R {
        throwIfLoadingShown()
        return coroutineScope(block)
    }

    override val dialogDataList: List<DialogData>
        get() = shownDataList

    override suspend fun showLoading() {
        withCoroutineScope {
            try {
                suspendCancellableCoroutine { continuation ->
                    launch {
                        pushDialogData(LoadingDialogDataImpl(continuation))
                    }
                }
            } finally {
                popDialogData()
            }
        }
    }

    override suspend fun <R> withLoading(block: suspend () -> R) = withCoroutineScope {
        val showLoadingJob = launch {
            showLoading()
        }

        val result = block()

        showLoadingJob.cancel()

        result
    }

    override suspend fun showConfirm(visuals: ConfirmDialogVisuals): ConfirmResult = withCoroutineScope {
        try {
            suspendCancellableCoroutine { continuation ->
                launch {
                    pushDialogData(ConfirmDialogDataImpl(visuals, continuation))
                }
            }
        } finally {
            popDialogData()
        }
    }

    override suspend fun showCustom(composable: @Composable (dismiss: () -> Unit) -> Unit) {
        withCoroutineScope {
            try {
                suspendCancellableCoroutine { continuation ->
                    launch {
                        pushDialogData(CustomDialogDataImpl(composable, continuation))
                    }
                }
            } finally {
                popDialogData()
            }
        }
    }

    companion object {
        fun buildConfirmDialogVisuals(
            title: String,
            content: String,
            markdown: Boolean,
            confirm: String?,
            dismiss: String?
        ): ConfirmDialogVisuals = ConfirmDialogVisualsImpl(title, content, markdown, confirm, dismiss)
    }
}

@Composable
fun rememberDialogHostState(): DialogHostState {
    return remember {
        DialogHostState()
    }
}

interface DialogHandle {
    val isShown: Boolean
    fun show()
    fun hide()
}

interface LoadingDialogHandle : DialogHandle {
    suspend fun <R> withLoading(block: suspend () -> R): R
    fun showLoading()
}

interface ConfirmDialogHandle : DialogHandle {
    val visuals: ConfirmDialogVisuals?

    fun showConfirm(
        title: String,
        content: String,
        markdown: Boolean = false,
        confirm: String? = null,
        dismiss: String? = null
    )

    suspend fun awaitConfirm(
        title: String,
        content: String,
        markdown: Boolean = false,
        confirm: String? = null,
        dismiss: String? = null
    ): ConfirmResult
}

private abstract class DialogHandleBase(
    protected val coroutineScope: CoroutineScope
) : DialogHandle {

    protected var shownJob: Job? = null

    override val isShown: Boolean
        get() = shownJob?.isActive == true

    protected fun cancelLastJob() {
        shownJob?.cancel()
        shownJob = null
    }

    final override fun show() {
        cancelLastJob()
        shownJob = coroutineScope.launch {
            performShow()
        }
    }

    final override fun hide() {
        cancelLastJob()
    }

    abstract suspend fun performShow()
}

private class LoadingDialogHandleImpl(
    coroutineScope: CoroutineScope,
    private val dialogHost: DialogHost,
) : LoadingDialogHandle, DialogHandleBase(coroutineScope) {
    override suspend fun <R> withLoading(block: suspend () -> R): R {
        cancelLastJob()
        return coroutineScope.async {
            dialogHost.withLoading(block)
        }.also { shownJob = it }.await()
    }

    override fun showLoading() {
        show()
    }

    override suspend fun performShow() {
        dialogHost.showLoading()
    }
}

private class ConfirmDialogHandleImpl(
    coroutineScope: CoroutineScope,
    private val dialogHost: DialogHost,
    override var visuals: ConfirmDialogVisuals? = null,
    private val onConfirm: () -> Unit = {},
    private val onDismiss: () -> Unit = {}
) : ConfirmDialogHandle, DialogHandleBase(coroutineScope) {
    private fun handleResult(result: ConfirmResult) {
        when (result) {
            ConfirmResult.Confirmed -> onConfirm()
            ConfirmResult.Canceled -> onDismiss()
        }
    }

    override fun showConfirm(
        title: String,
        content: String,
        markdown: Boolean,
        confirm: String?,
        dismiss: String?
    ) {
        visuals = DialogHostState.buildConfirmDialogVisuals(title, content, markdown, confirm, dismiss)
        show()
    }

    override suspend fun awaitConfirm(
        title: String,
        content: String,
        markdown: Boolean,
        confirm: String?,
        dismiss: String?
    ): ConfirmResult {
        cancelLastJob()
        val visuals = DialogHostState.buildConfirmDialogVisuals(title, content, markdown, confirm, dismiss)
        this.visuals = visuals
        return coroutineScope.async {
            dialogHost.showConfirm(visuals).also { handleResult(it) }
        }.also { shownJob = it }.await()
    }

    override suspend fun performShow() {
        visuals?.let {
            dialogHost.showConfirm(it).also { ret -> handleResult(ret) }
        }
    }

    companion object {
        fun Saver(
            coroutineScope: CoroutineScope,
            dialogHost: DialogHost,
            onConfirm: () -> Unit,
            onDismiss: () -> Unit
        ) = Saver<ConfirmDialogHandle, Pair<Boolean, ConfirmDialogVisuals?>>(
            save = {
                it.isShown to it.visuals
            },
            restore = { (isShown, visuals) ->
                ConfirmDialogHandleImpl(coroutineScope, dialogHost, visuals, onConfirm, onDismiss).apply {
                    if (isShown) {
                        show()
                    }
                }
            }
        )
    }
}

private class CustomDialogHandleImpl(
    coroutineScope: CoroutineScope,
    private val composable: @Composable (dismiss: () -> Unit) -> Unit,
    private val dialogHost: DialogHost,
) : DialogHandleBase(coroutineScope) {
    override suspend fun performShow() {
        dialogHost.showCustom(composable)
    }

    companion object {
        fun Saver(
            coroutineScope: CoroutineScope,
            composable: @Composable (dismiss: () -> Unit) -> Unit,
            dialogHost: DialogHost
        ) = Saver<DialogHandle, Boolean>(
            save = { it.isShown },
            restore = { isShown ->
                CustomDialogHandleImpl(coroutineScope, composable, dialogHost).apply {
                    if (isShown) {
                        show()
                    }
                }
            }
        )
    }
}

@Composable
fun rememberLoadingDialog(): LoadingDialogHandle {
    val coroutineScope = rememberCoroutineScope()
    val dialogHost = LocalDialogHost.current
    return remember {
        LoadingDialogHandleImpl(coroutineScope, dialogHost)
    }
}

@Composable
fun rememberConfirmDialog(
    onConfirm: (() -> Unit)? = null,
    onDismiss: (() -> Unit)? = null
): ConfirmDialogHandle {
    val coroutineScope = rememberCoroutineScope()
    val dialogHost = LocalDialogHost.current
    return if (onConfirm != null || onDismiss != null) {
        val wrappedConfirm: () -> Unit = {
            onConfirm?.invoke()
        }
        val wrappedDismiss: () -> Unit = {
            onDismiss?.invoke()
        }
        rememberSaveable(
            saver = ConfirmDialogHandleImpl.Saver(coroutineScope, dialogHost, onConfirm = wrappedConfirm, onDismiss = wrappedDismiss),
            init = {
                ConfirmDialogHandleImpl(coroutineScope, dialogHost, onConfirm = wrappedConfirm, onDismiss = wrappedDismiss)
            }
        )
    } else {
        remember {
            ConfirmDialogHandleImpl(coroutineScope, dialogHost)
        }
    }
}

@Composable
fun rememberCustomDialog(composable: @Composable (dismiss: () -> Unit) -> Unit): DialogHandle {
    val coroutineScope = rememberCoroutineScope()
    val dialogHost = LocalDialogHost.current
    return rememberSaveable(
        saver = CustomDialogHandleImpl.Saver(coroutineScope, composable, dialogHost),
        init = {
            CustomDialogHandleImpl(coroutineScope, composable, dialogHost)
        }
    )
}

@Composable
private fun LoadingDialog() {
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
private fun ConfirmDialog(confirmDialogData: ConfirmDialogData) {

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
private fun CustomDialog(customDialogData: CustomDialogData) {
    customDialogData.composable(customDialogData::dismiss)
}

@Composable
fun DialogHost(dialogHostState: DialogHostState = LocalDialogHost.current) {
    dialogHostState.dialogDataList.forEach { dialogData ->
        when (dialogData) {
            is LoadingDialogData -> LoadingDialog()
            is ConfirmDialogData -> ConfirmDialog(confirmDialogData = dialogData)
            is CustomDialogData -> CustomDialog(customDialogData = dialogData)
        }
    }
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
        }
    )
}