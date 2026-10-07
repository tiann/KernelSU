package me.weishu.kernelsu.ui.screen.flash

import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.SmallExtendedFloatingActionButton
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.key
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.terminal.TerminalSession
import me.weishu.kernelsu.ui.component.KeyEventBlocker
import me.weishu.kernelsu.ui.component.material.ExpressiveScaffold
import me.weishu.kernelsu.ui.component.material.SnackBarHost
import me.weishu.kernelsu.ui.component.material.TopBarBackButton
import me.weishu.kernelsu.ui.component.material.expressiveTopAppBarColors
import me.weishu.kernelsu.ui.terminal.TerminalScreen

@Composable
fun FlashScreenMaterial(
    state: FlashUiState,
    actions: FlashScreenActions,
    snackBarHost: SnackbarHostState,
    terminal: TerminalSession,
) {
    if (state.showJailbreakWarning) {
        JailbreakFlashWarningDialog(
            onConfirm = actions.onConfirmJailbreakWarning,
            onDismiss = actions.onDismissJailbreakWarning,
        )
    }

    ExpressiveScaffold(
        snackbarHost = {
            SnackBarHost(
                hostState = snackBarHost,
                modifier = Modifier.let { if (state.showRebootAction) it else it.safeDrawingPadding() })
        },
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        stringResource(
                            when (state.flashingStatus) {
                                FlashingStatus.FLASHING -> R.string.flashing
                                FlashingStatus.SUCCESS -> R.string.flash_success
                                FlashingStatus.FAILED -> R.string.flash_failed
                            }
                        )
                    )
                },
                colors = expressiveTopAppBarColors(),
                navigationIcon = {
                    TopBarBackButton(onClick = actions.onBack)
                },
                actions = {
                    IconButton(onClick = actions.onSaveLog) {
                        Icon(Icons.Filled.Save, stringResource(R.string.save_log))
                    }
                }
            )
        },
        floatingActionButton = {
            if (state.showRebootAction) {
                SmallExtendedFloatingActionButton(
                    onClick = actions.onReboot,
                    icon = { Icon(Icons.Filled.Refresh, null) },
                    text = { Text(stringResource(state.rebootLabelRes)) },
                    modifier = Modifier.padding(
                        bottom = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                WindowInsets.captionBar.asPaddingValues().calculateBottomPadding(),
                    )
                )
            }
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val layoutDirection = LocalLayoutDirection.current
        KeyEventBlocker {
            it.key == Key.VolumeDown || it.key == Key.VolumeUp
        }

        TerminalScreen(
            terminal = terminal,
            modifier = Modifier
                .fillMaxSize()
                .padding(
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection),
                    top = innerPadding.calculateTopPadding(),
                    bottom = 80.dp + WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                            WindowInsets.captionBar.asPaddingValues().calculateBottomPadding(),
                ),
        )
    }
}
