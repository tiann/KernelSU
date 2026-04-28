package me.weishu.kernelsu.ui.screen.settings

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.ScreenShape
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun SettingPagerWear(
    uiState: SettingsUiState,
    actions: SettingsScreenActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val isWatch = UiMode.isWatchDevice
    val horizontalPadding = wearHorizontalPadding()
    val enabledText =
        stringResource(if (uiState.checkUpdate) R.string.wear_state_on else R.string.wear_state_off)
    val moduleEnabledText =
        stringResource(if (uiState.checkModuleUpdate) R.string.wear_state_on else R.string.wear_state_off)

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.settings))
                }
            }
            if (!isWatch) {
                item {
                    val currentUiMode = UiMode.fromValue(uiState.uiMode)
                    val currentUiModeIndex = UiMode.entries.indexOf(currentUiMode).coerceAtLeast(0)
                    val nextUiModeIndex = (currentUiModeIndex + 1) % UiMode.entries.size
                    WearSettingsButton(
                        label = stringResource(R.string.settings_ui_mode),
                        secondaryLabel = currentUiMode.name,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = { actions.onSetUiModeIndex(nextUiModeIndex) }
                    )
                }
            }
            item {
                WearSettingsButton(
                    label = stringResource(R.string.settings_check_update),
                    secondaryLabel = enabledText,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = { actions.onSetCheckUpdate(!uiState.checkUpdate) }
                )
            }
            if (uiState.canUseKernelFeatures) {
                item {
                    WearSettingsButton(
                        label = stringResource(R.string.settings_module_check_update),
                        secondaryLabel = moduleEnabledText,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = { actions.onSetCheckModuleUpdate(!uiState.checkModuleUpdate) }
                    )
                }
            }
            item {
                val shapeLabel = if (uiState.screenShape == ScreenShape.Round)
                    stringResource(R.string.settings_screen_shape_round)
                else
                    stringResource(R.string.settings_screen_shape_square)
                WearSettingsButton(
                    label = stringResource(R.string.settings_screen_shape),
                    secondaryLabel = shapeLabel,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = {
                        val next = if (uiState.screenShape == ScreenShape.Round) ScreenShape.Square else ScreenShape.Round
                        actions.onSetScreenShape(next)
                    }
                )
            }
            if (uiState.canUseKernelFeatures) {
                item {
                    WearSettingsButton(
                        label = stringResource(R.string.settings_profile_template),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onOpenProfileTemplate
                    )
                }
            }
            item {
                WearSettingsButton(
                    label = stringResource(R.string.about),
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onOpenAbout
                )
            }
        }
    }
}

@Composable
private fun WearSettingsButton(
    label: String,
    secondaryLabel: String? = null,
    modifier: Modifier = Modifier,
    onClick: () -> Unit,
) {
    WearTonalButton(
        label = label,
        secondaryLabel = secondaryLabel,
        onClick = onClick,
        modifier = modifier,
    )
}
