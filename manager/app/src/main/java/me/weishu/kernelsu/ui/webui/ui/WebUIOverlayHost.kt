package me.weishu.kernelsu.ui.webui.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.key
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.webui.model.WebUIBanner
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.ui.component.OutdatedWebViewBannerMaterial
import me.weishu.kernelsu.ui.webui.ui.component.OutdatedWebViewBannerMiuix
import me.weishu.kernelsu.ui.webui.ui.component.dialog.WebUIDialogMaterial
import me.weishu.kernelsu.ui.webui.ui.component.dialog.WebUIDialogMiuix
import me.weishu.kernelsu.ui.webui.viewmodel.WebUIViewModel

@Composable
fun BoxScope.WebUIOverlayHost(
    state: WebUIState,
    viewModel: WebUIViewModel,
) {
    if (state.banners.isNotEmpty()) {
        Column(
            modifier = Modifier
                .align(Alignment.TopCenter)
                .statusBarsPadding()
                .padding(top = 8.dp)
                .fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            state.banners.forEach { banner ->
                key(banner.id) {
                    when (banner) {
                        is WebUIBanner.OutdatedWebView -> when (LocalUiMode.current) {
                            UiMode.Miuix -> OutdatedWebViewBannerMiuix(
                                currentVersion = banner.currentVersion,
                                requiredVersion = banner.requiredVersion,
                                onDismiss = { viewModel.dismissBanner(banner.id) },
                                onUpdateClick = viewModel::updateWebView
                            )

                            UiMode.Material -> OutdatedWebViewBannerMaterial(
                                currentVersion = banner.currentVersion,
                                requiredVersion = banner.requiredVersion,
                                onDismiss = { viewModel.dismissBanner(banner.id) },
                                onUpdateClick = viewModel::updateWebView
                            )
                        }
                    }
                }
            }
        }
    }

    state.activeDialog?.let { dialog ->
        when (LocalUiMode.current) {
            UiMode.Miuix -> WebUIDialogMiuix(dialog)
            UiMode.Material -> WebUIDialogMaterial(dialog)
        }
    }
}
