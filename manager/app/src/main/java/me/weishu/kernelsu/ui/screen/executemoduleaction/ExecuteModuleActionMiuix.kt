package me.weishu.kernelsu.ui.screen.executemoduleaction

import androidx.activity.compose.BackHandler
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.border
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
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Close
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.key
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KeyEventBlocker
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.rememberBlurBackdrop
import top.yukonga.miuix.kmp.basic.FloatingActionButton
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
    val threshold = with(LocalDensity.current) { 100.dp.toPx() }
    val fabVisible by remember {
        var previousScroll = 0
        var scrollDelta = 0f
        var visible = true
        derivedStateOf {
            val currentScroll = scrollState.value
            val delta = (currentScroll - previousScroll).toFloat()
            scrollDelta = (scrollDelta + delta).coerceIn(-threshold, threshold)
            previousScroll = currentScroll
            if (currentScroll <= 0) {
                visible = scrollState.maxValue <= 0
                scrollDelta = 0f
            } else if (!visible && scrollDelta >= threshold) {
                visible = true
                scrollDelta = 0f
            } else if (visible && scrollDelta <= -threshold) {
                visible = false
                scrollDelta = 0f
            }
            visible
        }
    }
    val offsetHeight by animateDpAsState(
        targetValue = if (fabVisible) 0.dp else 180.dp + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding(),
        animationSpec = tween(durationMillis = 350),
    )
    val enableBlur = LocalEnableBlur.current
    val backdrop = rememberBlurBackdrop(enableBlur)
    val blurActive = backdrop != null
    val barColor = if (blurActive) Color.Transparent else colorScheme.surface

    BackHandler(enabled = !state.isComplete) { }

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
        floatingActionButton = {
            if (state.isComplete) {
                FloatingActionButton(
                    containerColor = colorScheme.primary,
                    shadowElevation = 0.dp,
                    onClick = actions.onClose,
                    modifier = Modifier
                        .offset {
                            IntOffset(x = 0, y = offsetHeight.roundToPx())
                        }
                        .padding(
                            bottom = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                    WindowInsets.captionBar.asPaddingValues().calculateBottomPadding() + 20.dp,
                            end = 20.dp
                        )
                        .border(0.05.dp, colorScheme.outline.copy(alpha = 0.5f), CircleShape),
                    content = {
                        Icon(
                            Icons.Rounded.Close,
                            null,
                            Modifier.size(40.dp),
                            tint = colorScheme.onPrimary
                        )
                    },
                )
            }
        },
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
