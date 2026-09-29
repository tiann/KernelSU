package me.weishu.kernelsu.ui.webui.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
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
private fun HandleConfigurationChanges(runtime: WebUIRuntime) {
    val configuration = LocalConfiguration.current
    LaunchedEffect(configuration.fontScale, runtime.webView) {
        runtime.webView?.settings?.textZoom = (configuration.fontScale * 100).toInt()
    }
}
