package me.weishu.kernelsu.ui.webui.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.union
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import me.weishu.kernelsu.ui.webui.Insets
import me.weishu.kernelsu.ui.webui.WebUIRuntime
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.ui.component.WebViewContainer
import me.weishu.kernelsu.ui.webui.viewmodel.WebUIViewModel

@Composable
fun WebUIScreen(
    state: WebUIState,
    runtime: WebUIRuntime,
    viewModel: WebUIViewModel,
) {
    BackHandler(enabled = state.webCanGoBack) {
        runtime.webView?.goBack()
    }

    Box(modifier = Modifier.fillMaxSize()) {
        val webViewModifier = Modifier
            .fillMaxSize()
            .then(
                if (state.isEdgeToEdge && state.isInsetsSupported) {
                    Modifier
                } else {
                    Modifier.safeDrawingPadding()
                }
            )

        Box(modifier = webViewModifier) {
            runtime.webView?.let {
                WebViewContainer(
                    webView = it,
                    onUrlLoaded = viewModel::onHomePageLoaded
                )
            }
        }

        AnimatedVisibility(
            visible = state.isLoading,
            enter = fadeIn(),
            exit = fadeOut(animationSpec = tween(250))
        ) {
            WebUILoadingMask()
        }

        WebUIOverlayHost(
            state = state,
            viewModel = viewModel
        )
    }

    HandleWebViewLifecycle(runtime)
    HandleConfigurationChanges(runtime)
    HandleInsets(state, runtime)
}

@Composable
private fun HandleWebViewLifecycle(
    runtime: WebUIRuntime,
) {
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
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }
}

@Composable
private fun HandleInsets(state: WebUIState, runtime: WebUIRuntime) {
    val density = LocalDensity.current
    val layoutDirection = LocalLayoutDirection.current
    if (!state.isEdgeToEdge) return

    // The page opted into edge-to-edge by requesting the insets CSS, so publish the device
    // insets (including the display cutout) as CSS variables and let the page pad itself.
    val deviceInsets = WindowInsets.systemBars.union(WindowInsets.displayCutout)
    LaunchedEffect(density, layoutDirection, deviceInsets) {
        val newInsets = Insets(
            top = (deviceInsets.getTop(density) / density.density).toInt(),
            bottom = (deviceInsets.getBottom(density) / density.density).toInt(),
            left = (deviceInsets.getLeft(density, layoutDirection) / density.density).toInt(),
            right = (deviceInsets.getRight(density, layoutDirection) / density.density).toInt(),
        )
        runtime.webView?.evaluateJavascript(newInsets.js, null)
    }
}

@Composable
private fun HandleConfigurationChanges(runtime: WebUIRuntime) {
    val configuration = LocalConfiguration.current
    LaunchedEffect(configuration.fontScale, runtime.webView) {
        runtime.webView?.settings?.textZoom = (configuration.fontScale * 100).toInt()
    }
}
