package me.weishu.kernelsu.ui.screen.colorpalette

import android.annotation.SuppressLint
import android.os.Build
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.MenuOpen
import androidx.compose.material.icons.rounded.AspectRatio
import androidx.compose.material.icons.rounded.BlurOn
import androidx.compose.material.icons.rounded.CallToAction
import androidx.compose.material.icons.rounded.Colorize
import androidx.compose.material.icons.rounded.DesignServices
import androidx.compose.material.icons.rounded.RoundedCorner
import androidx.compose.material.icons.rounded.Style
import androidx.compose.material.icons.rounded.Wallpaper
import androidx.compose.material.icons.rounded.WaterDrop
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.materialkolor.PaletteStyle
import com.materialkolor.dynamiccolor.ColorSpec
import com.materialkolor.rememberDynamicColorScheme
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.miuix.ScaleDialog
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.keyColorOptions
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.rememberBlurBackdrop
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Slider
import top.yukonga.miuix.kmp.basic.SliderDefaults
import top.yukonga.miuix.kmp.basic.TabRow
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.blur.layerBackdrop
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.preference.ArrowPreference
import top.yukonga.miuix.kmp.preference.OverlayDropdownPreference
import top.yukonga.miuix.kmp.preference.SwitchPreference
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.theme.miuixShape
import top.yukonga.miuix.kmp.utils.overScrollVertical

@Composable
fun ColorPaletteScreenMiuix(
    state: ColorPaletteUiState,
    actions: ColorPaletteScreenActions,
) {
    val scrollBehavior = MiuixScrollBehavior()
    val enableBlurState = LocalEnableBlur.current
    val backdrop = rememberBlurBackdrop(enableBlurState)
    val blurActive = backdrop != null
    val barColor = if (blurActive) Color.Transparent else colorScheme.surface
    val uiState = state.uiState
    val currentColorMode = state.currentColorMode
    val isDark = currentColorMode.isDark || currentColorMode.isSystem && isSystemInDarkTheme()

    Scaffold(
        topBar = {
            BlurredBar(backdrop) {
                TopAppBar(
                    color = barColor,
                    title = stringResource(R.string.settings_theme),
                    navigationIcon = {
                        IconButton(
                            onClick = actions.onBack
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
                    scrollBehavior = scrollBehavior
                )
            }
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val showScaleDialog = rememberSaveable { mutableStateOf(false) }

        Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
            LazyColumn(
                modifier = Modifier
                    .fillMaxHeight()
                    .overScrollVertical()
                    .nestedScroll(scrollBehavior.nestedScrollConnection)
                    .padding(horizontal = 12.dp),
                contentPadding = innerPadding,
                overscrollEffect = null,
            ) {
                item {
                    Spacer(modifier = Modifier.height(32.dp))
                    ThemePreviewCardMiuix(
                        keyColor = uiState.keyColor,
                        isDark = isDark,
                        miuixMonet = uiState.miuixMonet,
                        enableFloatingBottomBar = uiState.enableFloatingBottomBar,
                        enableFloatingBottomBarBlur = uiState.enableFloatingBottomBarBlur,
                        paletteStyle = state.currentPaletteStyle,
                        colorSpec = state.currentColorSpec,
                    )
                    Spacer(modifier = Modifier.height(72.dp))

                    val themeItems = listOf(
                        stringResource(id = R.string.settings_theme_mode_system),
                        stringResource(id = R.string.settings_theme_mode_light),
                        stringResource(id = R.string.settings_theme_mode_dark),
                    )
                    TabRow(
                        tabs = themeItems,
                        selectedTabIndex = (if (uiState.themeMode >= 3) uiState.themeMode - 3 else uiState.themeMode).coerceIn(0, 2),
                        onTabSelected = { index ->
                            actions.onSetThemeMode(index)
                        },
                        height = 48.dp,
                    )

                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        SwitchPreference(
                            title = stringResource(id = R.string.settings_monet),
                            startAction = {
                                Icon(
                                    Icons.Rounded.Wallpaper,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_monet),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.miuixMonet,
                            onCheckedChange = {
                                actions.onSetMiuixMonet(it)
                            }
                        )

                        AnimatedVisibility(
                            visible = uiState.miuixMonet
                        ) {
                            Column {
                                val colorItems = listOf(
                                    stringResource(id = R.string.settings_key_color_default),
                                    stringResource(id = R.string.color_red),
                                    stringResource(id = R.string.color_pink),
                                    stringResource(id = R.string.color_purple),
                                    stringResource(id = R.string.color_deep_purple),
                                    stringResource(id = R.string.color_indigo),
                                    stringResource(id = R.string.color_blue),
                                    stringResource(id = R.string.color_cyan),
                                    stringResource(id = R.string.color_teal),
                                    stringResource(id = R.string.color_green),
                                    stringResource(id = R.string.color_yellow),
                                    stringResource(id = R.string.color_amber),
                                    stringResource(id = R.string.color_orange),
                                    stringResource(id = R.string.color_brown),
                                    stringResource(id = R.string.color_blue_grey),
                                    stringResource(id = R.string.color_sakura),
                                )
                                val colorValues = listOf(0) + keyColorOptions
                                OverlayDropdownPreference(
                                    title = stringResource(id = R.string.settings_key_color),
                                    items = colorItems,
                                    startAction = {
                                        Icon(
                                            Icons.Rounded.Colorize,
                                            modifier = Modifier.padding(end = 6.dp),
                                            contentDescription = stringResource(id = R.string.settings_key_color),
                                            tint = colorScheme.onBackground
                                        )
                                    },
                                    selectedIndex = colorValues.indexOf(uiState.keyColor).takeIf { it >= 0 } ?: 0,
                                    onSelectedIndexChange = { index ->
                                        actions.onSetKeyColor(colorValues[index])
                                    }
                                )

                                AnimatedVisibility(
                                    visible = uiState.keyColor != 0
                                ) {
                                    Column {
                                        val styles = PaletteStyle.entries
                                        OverlayDropdownPreference(
                                            title = stringResource(R.string.settings_color_style),
                                            startAction = {
                                                Icon(
                                                    Icons.Rounded.Style,
                                                    modifier = Modifier.padding(end = 6.dp),
                                                    contentDescription = stringResource(id = R.string.settings_color_style),
                                                    tint = colorScheme.onBackground
                                                )
                                            },
                                            items = styles.map { it.name },
                                            selectedIndex = styles.indexOfFirst { it.name == uiState.colorStyle }.coerceAtLeast(0),
                                            onSelectedIndexChange = { index ->
                                                actions.onSetColorStyle(styles[index].name)
                                            }
                                        )

                                        val specs = ColorSpec.SpecVersion.entries
                                        OverlayDropdownPreference(
                                            title = stringResource(R.string.settings_color_spec),
                                            startAction = {
                                                Icon(
                                                    Icons.Rounded.DesignServices,
                                                    modifier = Modifier.padding(end = 6.dp),
                                                    contentDescription = stringResource(id = R.string.settings_color_spec),
                                                    tint = colorScheme.onBackground
                                                )
                                            },
                                            items = specs.map { it.name },
                                            selectedIndex = specs.indexOfFirst { it.name == uiState.colorSpec }.coerceAtLeast(0),
                                            onSelectedIndexChange = { index ->
                                                actions.onSetColorSpec(specs[index].name)
                                            }
                                        )
                                    }
                                }
                            }
                        }
                    }

                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_enable_blur),
                                summary = stringResource(id = R.string.settings_enable_blur_summary),
                                startAction = {
                                    Icon(
                                        Icons.Rounded.BlurOn,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_enable_blur),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.enableBlur,
                                onCheckedChange = {
                                    actions.onSetEnableBlur(it)
                                }
                            )
                        }
                        SwitchPreference(
                            title = stringResource(id = R.string.settings_floating_bottom_bar),
                            summary = stringResource(id = R.string.settings_floating_bottom_bar_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.CallToAction,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_floating_bottom_bar),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.enableFloatingBottomBar,
                            onCheckedChange = {
                                actions.onSetEnableFloatingBottomBar(it)
                            }
                        )
                        AnimatedVisibility(visible = uiState.enableFloatingBottomBar && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_enable_glass),
                                summary = stringResource(id = R.string.settings_enable_glass_summary),
                                startAction = {
                                    Icon(
                                        Icons.Rounded.WaterDrop,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_enable_glass),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.enableFloatingBottomBarBlur,
                                onCheckedChange = {
                                    actions.onSetEnableFloatingBottomBarBlur(it)
                                }
                            )
                        }
                        SwitchPreference(
                            title = stringResource(id = R.string.settings_smooth_corner),
                            summary = stringResource(id = R.string.settings_smooth_corner_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.RoundedCorner,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_smooth_corner),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.enableSmoothCorner,
                            onCheckedChange = {
                                actions.onSetEnableSmoothCorner(it)
                            }
                        )
                    }

                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_enable_predictive_back),
                                summary = stringResource(id = R.string.settings_enable_predictive_back_summary),
                                startAction = {
                                    Icon(
                                        Icons.AutoMirrored.Rounded.MenuOpen,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_enable_predictive_back),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.enablePredictiveBack,
                                onCheckedChange = {
                                    actions.onSetEnablePredictiveBack(it)
                                }
                            )
                        }

                        var sliderValue by remember(uiState.pageScale) { mutableFloatStateOf(uiState.pageScale) }
                        ArrowPreference(
                            title = stringResource(id = R.string.settings_page_scale),
                            summary = stringResource(id = R.string.settings_page_scale_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.AspectRatio,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_page_scale),
                                    tint = colorScheme.onBackground
                                )
                            },
                            endActions = {
                                Text(
                                    text = "${(sliderValue * 100).toInt()}%",
                                    color = colorScheme.onSurfaceVariantActions,
                                )
                            },
                            onClick = { showScaleDialog.value = !showScaleDialog.value },
                            holdDownState = showScaleDialog.value,
                            bottomAction = {
                                Slider(
                                    value = sliderValue,
                                    onValueChange = {
                                        sliderValue = it
                                    },
                                    onValueChangeFinished = {
                                        actions.onSetPageScale(sliderValue)
                                    },
                                    valueRange = 0.8f..1.1f,
                                    showKeyPoints = true,
                                    keyPoints = listOf(0.8f, 0.9f, 1f, 1.1f),
                                    magnetThreshold = 0.01f,
                                    hapticEffect = SliderDefaults.SliderHapticEffect.Step,
                                )
                            },
                        )
                        ScaleDialog(
                            show = showScaleDialog.value,
                            onDismissRequest = { showScaleDialog.value = false },
                            volumeState = { uiState.pageScale },
                            onVolumeChange = {
                                actions.onSetPageScale(it)
                            }
                        )
                    }
                }
                item {
                    Spacer(
                        Modifier.height(
                            WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                    WindowInsets.captionBar.asPaddingValues().calculateBottomPadding() +
                                    12.dp
                        )
                    )
                }
            }
        }
    }
}

@SuppressLint("ConfigurationScreenWidthHeight")
@Composable
private fun ThemePreviewCardMiuix(
    keyColor: Int,
    isDark: Boolean,
    miuixMonet: Boolean,
    enableFloatingBottomBar: Boolean = false,
    enableFloatingBottomBarBlur: Boolean = false,
    paletteStyle: PaletteStyle = PaletteStyle.TonalSpot,
    colorSpec: ColorSpec.SpecVersion = ColorSpec.SpecVersion.SPEC_2021,
) {
    val configuration = LocalConfiguration.current
    val screenWidth = configuration.screenWidthDp.toFloat()
    val screenHeight = configuration.screenHeightDp.toFloat()
    val screenRatio = screenWidth / screenHeight

    val seedColor = if (keyColor == 0) colorScheme.primary else Color(keyColor)
    val effectiveStyle = if (keyColor == 0) PaletteStyle.TonalSpot else paletteStyle
    val effectiveSpec = if (keyColor == 0) ColorSpec.SpecVersion.Default else colorSpec
    val dynamicCs = rememberDynamicColorScheme(
        seedColor = seedColor,
        isDark = isDark,
        style = effectiveStyle,
        specVersion = effectiveSpec,
    )

    val bgColor = if (miuixMonet) dynamicCs.background else colorScheme.surface
    val textColor = if (miuixMonet) dynamicCs.onSurface else colorScheme.onBackground
    val accentCardColor = when {
        miuixMonet -> dynamicCs.secondaryContainer
        isDark -> Color(0xFF1A3825)
        else -> Color(0xFFDFFAE4)
    }
    val cardColor = if (miuixMonet) dynamicCs.surfaceContainerHighest else colorScheme.surfaceVariant
    val navBarColor = if (miuixMonet) dynamicCs.surfaceContainer else colorScheme.surface
    val iconColor = if (miuixMonet) dynamicCs.primary else colorScheme.primary
    val navSelectedColor = colorScheme.onSurfaceContainer
    val navUnselectedColor = colorScheme.onSurfaceContainer.copy(alpha = 0.5f)

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 12.dp),
        contentAlignment = Alignment.TopCenter
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth(0.4f)
                .aspectRatio(screenRatio)
                .clip(miuixShape(20.dp))
                .background(bgColor)
                .border(1.dp, colorScheme.outline, miuixShape(20.dp))
        ) {
            Column {
                Row(
                    modifier = Modifier
                        .height(48.dp)
                        .fillMaxWidth()
                        .padding(start = 12.dp, top = 24.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = stringResource(id = R.string.app_name),
                        fontSize = 12.sp,
                        color = textColor
                    )
                }

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(65.dp)
                        .padding(horizontal = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight()
                            .clip(RoundedCornerShape(6.dp))
                            .background(accentCardColor)
                    )
                    Column(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight(),
                        verticalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f)
                                .clip(RoundedCornerShape(6.dp))
                                .background(cardColor)
                        )
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f)
                                .clip(RoundedCornerShape(6.dp))
                                .background(cardColor)
                        )
                    }
                }

                Column(
                    modifier = Modifier
                        .weight(1f)
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(0.8f)
                            .clip(RoundedCornerShape(6.dp))
                            .background(cardColor)
                    )
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(.1f)
                            .clip(RoundedCornerShape(6.dp))
                            .background(cardColor)
                    )
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(.1f)
                            .clip(RoundedCornerShape(6.dp))
                            .background(cardColor)
                    )
                }

            }

            if (enableFloatingBottomBar) {
                Box(
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 8.dp),
                ) {
                    Row(
                        modifier = Modifier
                            .height(28.dp)
                            .clip(RoundedCornerShape(14.dp))
                            .background(
                                if (enableFloatingBottomBarBlur) navBarColor.copy(alpha = 0.5f)
                                else navBarColor
                            )
                            .border(0.5.dp, textColor.copy(alpha = 0.1f), RoundedCornerShape(14.dp))
                            .padding(horizontal = 12.dp),
                        horizontalArrangement = Arrangement.spacedBy(10.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        repeat(4) {
                            Box(
                                modifier = Modifier
                                    .size(13.dp)
                                    .clip(RoundedCornerShape(2.dp))
                                    .background(if (it == 0) iconColor else textColor)
                            )
                        }
                    }
                }
            } else {
                Column(
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .fillMaxWidth()
                ) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(0.5.dp)
                            .background(textColor.copy(alpha = 0.1f))
                    )
                    Row(
                        modifier = Modifier
                            .height(36.dp)
                            .fillMaxWidth()
                            .background(navBarColor)
                            .padding(top = 2.dp, bottom = 8.dp),
                        horizontalArrangement = Arrangement.SpaceEvenly,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        repeat(4) {
                            Box(
                                modifier = Modifier
                                    .size(15.dp)
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (it == 0) navSelectedColor else navUnselectedColor)
                            )
                        }
                    }
                }
            }
        }
    }
}
