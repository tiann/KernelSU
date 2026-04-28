package me.weishu.kernelsu.ui.screen.install

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearPrimaryButton
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
internal fun InstallScreenWear(
    state: InstallUiState,
    actions: InstallScreenActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()
    val selectedFile = (state.installMethod as? InstallMethod.SelectFile)?.uri?.lastPathSegment

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.install))
                }
            }

            state.installMethodOptions.forEach { method ->
                item {
                    val isSelected = method::class == state.installMethod?.let { it::class }
                    val onClick = {
                        when (method) {
                            is InstallMethod.SelectFile -> actions.onSelectBootImage()
                            else -> actions.onSelectMethod(method)
                        }
                    }
                    if (isSelected) {
                        WearPrimaryButton(
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            onClick = onClick,
                            label = method.label(),
                            secondaryLabel = method.summary,
                        )
                    } else {
                        WearTonalButton(
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            onClick = onClick,
                            label = method.label(),
                            secondaryLabel = method.summary,
                        )
                    }
                }
            }

            selectedFile?.let { fileName ->
                item {
                    WearMessageText(
                        text = fileName,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            }

            if (state.currentKmi.isNotBlank()) {
                item {
                    WearMessageText(
                        text = "KMI: ${state.currentKmi}",
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            }

            item {
                WearPrimaryButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onNext,
                    label = stringResource(R.string.install_next),
                    enabled = state.installMethod != null,
                )
            }
        }
    }
}

@Composable
private fun InstallMethod.label(): String {
    return when (this) {
        is InstallMethod.DirectInstall -> stringResource(R.string.direct_install)
        is InstallMethod.DirectInstallToInactiveSlot -> stringResource(R.string.install_inactive_slot)
        is InstallMethod.SelectFile -> stringResource(R.string.select_file)
    }
}
