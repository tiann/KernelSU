package me.weishu.kernelsu.ui.webui.ui

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.runtime.WebUIRuntime

@Composable
fun WebUIScreen(
    state: WebUIState,
    runtime: WebUIRuntime,
    dispatch: (WebUIIntent) -> Unit,
) {
    val innerPadding = rememberWebUIContentPadding(state)
    val showExternalLink = state.externalLinkUrl != null

    BackHandler(enabled = showExternalLink) {
        dispatch(WebUIIntent.ExternalLinkGoBack)
    }

    BackHandler(enabled = state.webCanGoBack && !showExternalLink) {
        runtime.webView?.goBack()
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(innerPadding)
    ) {
        WebViewContainer(state, runtime, dispatch)
    }

    ExternalLinkWarningOverlay(
        externalLinkUrl = state.externalLinkUrl,
        runtime = runtime,
        dispatch = dispatch,
    )

    HandleWebUIEvent(state, dispatch)
    SyncWebUIInsets(state, dispatch)
    HandleWebViewLifecycle(runtime)
    HandleConfigurationChanges(runtime)
}
