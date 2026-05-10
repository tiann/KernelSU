package me.weishu.kernelsu.ui.screen.flash

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
fun FlashScreenWear(
    state: FlashUiState,
    actions: FlashScreenActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(
                        when (state.flashingStatus) {
                            FlashingStatus.FLASHING -> stringResource(R.string.flashing)
                            FlashingStatus.SUCCESS -> stringResource(R.string.flash_success)
                            FlashingStatus.FAILED -> stringResource(R.string.flash_failed)
                        }
                    )
                }
            }

            if (state.showJailbreakWarning) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.jailbreak_flash_warning),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        isError = true,
                    )
                }
                item {
                    WearPrimaryButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onConfirmJailbreakWarning,
                        label = stringResource(R.string.confirm),
                    )
                }
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onDismissJailbreakWarning,
                        label = stringResource(R.string.cancel),
                    )
                }
            } else {
                item {
                    WearMessageText(
                        text = state.text,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }

                if (state.flashingStatus != FlashingStatus.FLASHING) {
                    item {
                        WearTonalButton(
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            onClick = actions.onSaveLog,
                            label = stringResource(R.string.save_log),
                        )
                    }
                }

                if (state.showRebootAction) {
                    item {
                        WearPrimaryButton(
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            onClick = actions.onReboot,
                            label = stringResource(R.string.reboot),
                        )
                    }
                }
            }
        }
    }
}
