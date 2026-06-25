package me.weishu.kernelsu.ui.webui.ui

import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.ime
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.union
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import me.weishu.kernelsu.ui.webui.model.Insets
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.runtime.WebUIRuntime

@Composable
internal fun rememberWebUIContentPadding(webUIState: WebUIState): androidx.compose.foundation.layout.PaddingValues {
    val drawingInsets = WindowInsets.safeDrawing
    val imeInsets = WindowInsets.ime
    return if (webUIState.isInsetsEnabled) imeInsets.asPaddingValues() else drawingInsets.asPaddingValues()
}

@Composable
internal fun SyncWebUIInsets(
    webUIState: WebUIState,
    dispatch: (WebUIIntent) -> Unit,
) {
    val density = LocalDensity.current
    val layoutDirection = LocalLayoutDirection.current
    val systemBarsInsets = WindowInsets.systemBars.union(WindowInsets.displayCutout)

    LaunchedEffect(density, layoutDirection, systemBarsInsets, webUIState.isInsetsEnabled) {
        if (!webUIState.isInsetsEnabled) {
            return@LaunchedEffect
        }
        snapshotFlow {
            val top = (systemBarsInsets.getTop(density) / density.density).toInt()
            val bottom = (systemBarsInsets.getBottom(density) / density.density).toInt()
            val left = (systemBarsInsets.getLeft(density, layoutDirection) / density.density).toInt()
            val right = (systemBarsInsets.getRight(density, layoutDirection) / density.density).toInt()
            Insets(top, bottom, left, right)
        }.collect { newInsets ->
            if (webUIState.currentInsets != newInsets) {
                dispatch(WebUIIntent.InsetsChanged(newInsets))
            }
        }
    }
}

@Composable
internal fun HandleWebViewLifecycle(runtime: WebUIRuntime) {
    val lifecycleOwner = LocalLifecycleOwner.current

    DisposableEffect(lifecycleOwner, runtime) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> runtime.webView?.onResume()
                Lifecycle.Event.ON_PAUSE -> runtime.webView?.onPause()
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
internal fun HandleConfigurationChanges(runtime: WebUIRuntime) {
    val configuration = LocalConfiguration.current
    LaunchedEffect(configuration.fontScale, runtime.webView) {
        runtime.webView?.settings?.textZoom = (configuration.fontScale * 100).toInt()
    }
}
