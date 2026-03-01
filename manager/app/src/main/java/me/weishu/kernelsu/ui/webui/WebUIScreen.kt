package me.weishu.kernelsu.ui.webui

import android.app.Activity
import android.net.Uri
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.input.rememberTextFieldState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import me.weishu.kernelsu.R
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.extra.WindowDialog


@Composable
fun WebUIScreen(webUIState: WebUIState) {
    val density = LocalDensity.current
    val layoutDirection = LocalLayoutDirection.current
    val windowInsets = WindowInsets.safeDrawing
    val innerPadding = if (webUIState.isInsetsEnabled) PaddingValues(0.dp) else windowInsets.asPaddingValues()

    LaunchedEffect(density, layoutDirection, windowInsets, webUIState.isInsetsEnabled) {
        if (!webUIState.isInsetsEnabled) {
            return@LaunchedEffect
        }
        snapshotFlow {
            val top = (windowInsets.getTop(density) / density.density).toInt()
            val bottom = (windowInsets.getBottom(density) / density.density).toInt()
            val left = (windowInsets.getLeft(density, layoutDirection) / density.density).toInt()
            val right = (windowInsets.getRight(density, layoutDirection) / density.density).toInt()
            Insets(top, bottom, left, right)
        }.collect { newInsets ->
            if (webUIState.currentInsets != newInsets) {
                webUIState.currentInsets = newInsets
                webUIState.webView?.evaluateJavascript(newInsets.js, null)
            }
        }
    }

    BackHandler(enabled = webUIState.webCanGoBack) {
        webUIState.webView?.goBack()
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(innerPadding)
    ) {
        if (webUIState.webView != null) {
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { _ ->
                    webUIState.webView!!.apply {
                        layoutParams = ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT
                        )
                        if (!webUIState.isUrlLoaded) {
                            val homePage = "https://mui.kernelsu.org/index.html"
                            if (width > 0 && height > 0) {
                                loadUrl(homePage)
                                webUIState.isUrlLoaded = true
                            } else {
                                val listener = object : View.OnLayoutChangeListener {
                                    override fun onLayoutChange(
                                        v: View, left: Int, top: Int, right: Int, bottom: Int,
                                        oldLeft: Int, oldTop: Int, oldRight: Int, oldBottom: Int
                                    ) {
                                        if (v.width > 0 && v.height > 0) {
                                            (v as WebView).loadUrl(homePage)
                                            webUIState.isUrlLoaded = true
                                            v.removeOnLayoutChangeListener(this)
                                        }
                                    }
                                }
                                addOnLayoutChangeListener(listener)
                            }
                        }
                    }
                }
            )
        }
    }

    HandleWebUIEvent(webUIState)
    HandleWebViewLifecycle(webUIState)
    HandleConfigurationChanges(webUIState)
}

@Composable
private fun HandleWebUIEvent(webUIState: WebUIState) {

    val fileLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { result ->
        val uris: Array<Uri>? = if (result.resultCode == Activity.RESULT_OK) {
            result.data?.let { data ->
                if (data.clipData != null) {
                    Array(data.clipData!!.itemCount) { i -> data.clipData!!.getItemAt(i).uri }
                } else {
                    data.data?.let { arrayOf(it) }
                }
            }
        } else null
        webUIState.onFileChooserResult(uris)
    }

    when (val event = webUIState.uiEvent) {
        is WebUIEvent.ShowAlert -> {
            val showDialog = remember(event) { mutableStateOf(true) }
            WindowDialog(
                onDismissRequest = { },
                show = showDialog,
            ) {
                Column {
                    Text(event.message)
                    Spacer(Modifier.height(12.dp))
                    TextButton(
                        modifier = Modifier.fillMaxWidth(), onClick = {
                            webUIState.onAlertResult()
                            showDialog.value = false
                        }, text = stringResource(R.string.confirm), colors = ButtonDefaults.textButtonColorsPrimary()
                    )
                }
            }
        }

        is WebUIEvent.ShowConfirm -> {
            val showDialog = remember(event) { mutableStateOf(true) }
            WindowDialog(
                onDismissRequest = { webUIState.onConfirmResult(false) },
                show = showDialog,
            ) {
                Column {
                    Text(event.message)
                    Spacer(Modifier.height(12.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        TextButton(
                            onClick = {
                                webUIState.onConfirmResult(false)
                                showDialog.value = false
                            },
                            text = stringResource(android.R.string.cancel),
                            modifier = Modifier.weight(1f),
                        )
                        Spacer(modifier = Modifier.width(20.dp))
                        TextButton(
                            onClick = {
                                webUIState.onConfirmResult(true)
                                showDialog.value = false
                            },
                            text = stringResource(R.string.confirm),
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.textButtonColorsPrimary()
                        )
                    }
                }
            }
        }

        is WebUIEvent.ShowPrompt -> {
            val showDialog = remember(event) { mutableStateOf(true) }
            val state = rememberTextFieldState(event.defaultValue)
            WindowDialog(
                onDismissRequest = { webUIState.onPromptResult(null) },
                show = showDialog,
            ) {
                Column {
                    Text(event.message)
                    Spacer(Modifier.height(12.dp))
                    TextField(
                        modifier = Modifier.padding(bottom = 16.dp), state = state
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        TextButton(
                            onClick = {
                                webUIState.onPromptResult(null)
                                showDialog.value = false
                            },
                            text = stringResource(android.R.string.cancel),
                            modifier = Modifier.weight(1f),
                        )
                        Spacer(modifier = Modifier.width(20.dp))
                        TextButton(
                            onClick = {
                                webUIState.onPromptResult(state.text.toString())
                                showDialog.value = false
                            },
                            text = stringResource(R.string.confirm),
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.textButtonColorsPrimary()
                        )
                    }
                }
            }
        }

        is WebUIEvent.ShowFileChooser -> {
            LaunchedEffect(event) {
                try {
                    fileLauncher.launch(event.intent)
                } catch (_: Exception) {
                    webUIState.onFileChooserResult(null)
                }
            }
        }

        else -> {}
    }
}

@Composable
private fun HandleWebViewLifecycle(webUIState: WebUIState) {
    val lifecycleOwner = LocalLifecycleOwner.current

    DisposableEffect(lifecycleOwner, webUIState) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> webUIState.webView?.onResume()
                Lifecycle.Event.ON_PAUSE -> webUIState.webView?.onPause()
                else -> {}
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)

        onDispose {
            lifecycleOwner.lifecycle.removeObserver(observer)
        }
    }
}

@Composable
private fun HandleConfigurationChanges(webUIState: WebUIState) {
    val configuration = LocalConfiguration.current
    LaunchedEffect(configuration.fontScale, webUIState.webView) {
        webUIState.webView?.settings?.textZoom = (configuration.fontScale * 100).toInt()
    }
}