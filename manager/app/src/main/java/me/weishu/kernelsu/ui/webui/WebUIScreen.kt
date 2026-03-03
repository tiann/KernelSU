package me.weishu.kernelsu.ui.webui

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun rememberFileLauncher(webUIState: WebUIState): ActivityResultLauncher<Intent> {
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
        webUIState.onFileChooserResult(uris)
    }
}

@Composable
fun WebUIScreen(webUIState: WebUIState) {
    val density = LocalDensity.current
    val layoutDirection = LocalLayoutDirection.current
    val windowInsets = WindowInsets.safeDrawing
    val innerPadding = if (webUIState.isInsetsEnabled) PaddingValues(0.dp) else windowInsets.asPaddingValues()
    val fileLauncher = rememberFileLauncher(webUIState)

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
        webUIState.webView?.let { webView ->
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { _ ->
                    webView.apply {
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

    when (LocalUiMode.current) {
        UiMode.Miuix -> HandleWebUIEventMiuix(webUIState, fileLauncher)
        UiMode.Material -> HandleWebUIEventMaterial(webUIState, fileLauncher)
    }

    HandleWebViewLifecycle(webUIState)
    HandleConfigurationChanges(webUIState)
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
