package me.weishu.kernelsu.ui.wear

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route

@Composable
fun WearMainScreen(
    navigator: Navigator,
    canUseKernelFeatures: Boolean,
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
                    Text(stringResource(R.string.app_name))
                }
            }
            item {
                WearMenuButton(
                    label = R.string.home,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                ) {
                    navigator.push(Route.Home)
                }
            }
            if (canUseKernelFeatures) {
                item {
                    WearMenuButton(
                        label = R.string.superuser,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    ) {
                        navigator.push(Route.SuperUser)
                    }
                }
                item {
                    WearMenuButton(
                        label = R.string.module,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    ) {
                        navigator.push(Route.Module)
                    }
                }
            }
            item {
                WearMenuButton(
                    label = R.string.settings,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                ) {
                    navigator.push(Route.Settings)
                }
            }
        }
    }
}

@Composable
fun WearPlaceholderScreen(
    @StringRes title: Int,
    message: String,
    actionLabel: String? = null,
    onAction: (() -> Unit)? = null,
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
                    Text(stringResource(title))
                }
            }
            item {
                WearMessageText(
                    text = message,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                )
            }
            if (actionLabel != null && onAction != null) {
                item {
                    WearPrimaryButton(
                        label = actionLabel,
                        onClick = onAction,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            }
        }
    }
}

@Composable
fun WearInstallRequiredScreen(
    @StringRes title: Int,
    canInstall: Boolean,
    onInstallClick: () -> Unit,
) {
    WearPlaceholderScreen(
        title = title,
        message = stringResource(
            if (canInstall) R.string.wear_install_required else R.string.wear_feature_unavailable
        ),
        actionLabel = if (canInstall) stringResource(R.string.home_click_to_install) else null,
        onAction = if (canInstall) onInstallClick else null,
    )
}

@Composable
private fun WearMenuButton(
    @StringRes label: Int,
    modifier: Modifier = Modifier,
    onClick: () -> Unit,
) {
    WearTonalButton(
        label = stringResource(label),
        onClick = onClick,
        modifier = modifier,
    )
}
