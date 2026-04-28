package me.weishu.kernelsu.ui.screen.home

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Card
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun HomePagerWear(
    state: HomeUiState,
    actions: HomeActions,
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
                WearStatusCard(
                    state = state,
                    onInstallClick = actions.onInstallClick,
                )
            }

            if (state.isFullFeatured) {
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onSuperuserClick,
                        label = stringResource(R.string.superuser),
                        secondaryLabel = state.superuserCount.toString(),
                    )
                }
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onModuleClick,
                        label = stringResource(R.string.module),
                        secondaryLabel = state.moduleCount.toString(),
                    )
                }
            }

            if (state.showGkiWarning) {
                item { WearWarningCard(stringResource(R.string.home_gki_warning)) }
            }
            if (state.showRequireKernelWarning) {
                item {
                    WearWarningCard(
                        stringResource(
                            R.string.require_kernel_version,
                            state.ksuVersion ?: 0,
                            me.weishu.kernelsu.Natives.MINIMAL_SUPPORTED_KERNEL
                        )
                    )
                }
            }
            if (state.showRootWarning) {
                item { WearWarningCard(stringResource(R.string.grant_root_failed)) }
            }

            item {
                WearInfoCard(state.systemInfo)
            }

            if (state.ksuVersion == null && !state.isLateLoadMode) {
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onInstallClick,
                        label = stringResource(R.string.home_click_to_install),
                    )
                }
            }
        }
    }
}

@Composable
private fun WearStatusCard(
    state: HomeUiState,
    onInstallClick: () -> Unit,
) {
    val horizontalPadding = wearHorizontalPadding()
    val canInstall = state.ksuVersion == null && !state.isLateLoadMode
    val (title, subtitle) = when {
        state.ksuVersion != null -> {
            val mode = when (state.lkmMode) {
                true -> " (LKM)"
                false -> " (GKI)"
                null -> ""
            }
            stringResource(R.string.home_working) + mode to
                    stringResource(R.string.home_working_version, state.ksuVersion)
        }

        state.kernelVersion.isGKI() ->
            stringResource(R.string.home_not_installed) to stringResource(R.string.home_click_to_install)

        else ->
            stringResource(R.string.home_unsupported) to stringResource(R.string.home_unsupported_reason)
    }

    Card(
        onClick = {
            if (canInstall) onInstallClick()
        },
        modifier = Modifier
            .padding(horizontal = horizontalPadding)
            .fillMaxWidth(),
    ) {
        Text(text = title, style = MaterialTheme.typography.titleSmall)
        Text(text = subtitle, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun WearWarningCard(message: String) {
    val horizontalPadding = wearHorizontalPadding()
    Card(
        onClick = {},
        modifier = Modifier
            .padding(horizontal = horizontalPadding)
            .fillMaxWidth(),
    ) {
        Text(
            text = message,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.error,
        )
    }
}

@Composable
private fun WearInfoCard(systemInfo: SystemInfo) {
    val horizontalPadding = wearHorizontalPadding()
    Card(
        onClick = {},
        modifier = Modifier
            .padding(horizontal = horizontalPadding)
            .fillMaxWidth(),
    ) {
        Text(
            text = stringResource(R.string.home_kernel),
            style = MaterialTheme.typography.labelSmall,
        )
        Text(
            text = systemInfo.kernelVersion,
            style = MaterialTheme.typography.bodySmall,
        )
        Text(
            text = stringResource(R.string.home_manager_version),
            style = MaterialTheme.typography.labelSmall,
        )
        Text(
            text = systemInfo.managerVersion,
            style = MaterialTheme.typography.bodySmall,
        )
    }
}
