package me.weishu.kernelsu.ui.screen.executemoduleaction

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.key
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KeyEventBlocker
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.rememberBlurBackdrop
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.SmallTopAppBar
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.blur.LayerBackdrop
import top.yukonga.miuix.kmp.blur.layerBackdrop
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.icon.extended.Download
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun ExecuteModuleActionScreenMiuix(
    state: ExecuteModuleActionUiState,
    actions: ExecuteModuleActionScreenActions,
) {
    val scrollState = rememberScrollState()
    val enableBlur = LocalEnableBlur.current
    val backdrop = rememberBlurBackdrop(enableBlur)
    val blurActive = backdrop != null
    val barColor = if (blurActive) Color.Transparent else colorScheme.surface

    BackHandler { }

    Scaffold(
        topBar = {
            TopBar(
                onBack = actions.onBack,
                onSave = actions.onSaveLog,
                backdrop = backdrop,
                barColor = barColor,
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout)
            .only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val layoutDirection = LocalLayoutDirection.current
        KeyEventBlocker {
            it.key == Key.VolumeDown || it.key == Key.VolumeUp
        }
        Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
            Column(
                modifier = Modifier
                    .fillMaxSize(1f)
                    .scrollEndHaptic()
                    .padding(
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection),
                    )
                    .verticalScroll(scrollState),
            ) {
                LaunchedEffect(state.text) {
                    scrollState.animateScrollTo(scrollState.maxValue)
                }
                Spacer(Modifier.height(innerPadding.calculateTopPadding()))
                Text(
                    modifier = Modifier.padding(8.dp),
                    text = state.text,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                )
                Spacer(
                    Modifier.height(
                        12.dp + WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                WindowInsets.captionBar.asPaddingValues().calculateBottomPadding()
                    )
                )
            }
        }
    }
}

@Composable
private fun TopBar(
    onBack: () -> Unit = {},
    onSave: () -> Unit = {},
    backdrop: LayerBackdrop?,
    barColor: Color,
) {
    BlurredBar(backdrop) {
        SmallTopAppBar(
            color = barColor,
            title = stringResource(R.string.action),
            navigationIcon = {
                IconButton(
                    onClick = onBack
                ) {
                    val layoutDirection = LocalLayoutDirection.current
                    Icon(
                        modifier = Modifier.graphicsLayer {
                            if (layoutDirection == LayoutDirection.Rtl) scaleX = -1f
                        },
                        imageVector = MiuixIcons.Back,
                        contentDescription = null,
                        tint = colorScheme.onBackground
                    )
                }
            },
            actions = {
                IconButton(
                    onClick = onSave
                ) {
                    Icon(
                        imageVector = MiuixIcons.Download,
                        contentDescription = stringResource(id = R.string.save_log),
                        tint = colorScheme.onBackground
                    )
                }
            }
        )
    }
}
