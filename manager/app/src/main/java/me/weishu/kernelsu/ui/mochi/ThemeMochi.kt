package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.screen.colorpalette.ColorPaletteScreenActions
import me.weishu.kernelsu.ui.screen.colorpalette.ColorPaletteUiState
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiStepper
import ui.feice.mochi.basic.MochiSwitch
import ui.feice.mochi.effect.content.crossfadeOrChange
import ui.feice.mochi.effect.content.revealIfShadeChanges
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListDefaults
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.MochiListSliderRow
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold
import ui.feice.mochi.picker.MochiSeedPalette
import ui.feice.mochi.theme.MochiTheme
import ui.feice.mochi.theme.PaletteStyle

private val PaletteChoices = listOf(PaletteStyle.TonalSpot, PaletteStyle.Expressive, PaletteStyle.Vibrant, PaletteStyle.Neutral)

@Composable
fun ThemeMochi(state: ColorPaletteUiState, actions: ColorPaletteScreenActions) {
    val context = LocalContext.current
    val appearance = MochiAppearance.get(context)
    val reveal = LocalMochiReveal.current
    val uiState = state.uiState
    val systemDark = isSystemInDarkTheme()
    val nowDark = MochiTheme.colorScheme.isDark
    var confirmGlass by rememberSaveable { mutableStateOf(false) }

    val modeIndex = (if (uiState.themeMode >= 3) uiState.themeMode - 3 else uiState.themeMode).coerceIn(0, 2)
    val modeLabels = listOf(
        stringResource(R.string.settings_theme_mode_system),
        stringResource(R.string.settings_theme_mode_light),
        stringResource(R.string.settings_theme_mode_dark),
    ).toImmutableList()
    val paletteLabels = PaletteChoices.map { it.name }.toImmutableList()

    MochiLazyCompactTitleScaffold(
        title = stringResource(R.string.settings_theme),
        navigationIcon = { MochiBackButton() },
    ) {
        item(key = "ui") {
            MochiListSection(footer = stringResource(R.string.mochi_ui_mode_footer)) {
                MochiListPickerRow(
                    stringResource(R.string.settings_ui_mode),
                    options = UiMode.entries.map { it.name }.toImmutableList(),
                    selectedIndex = UiMode.entries.indexOf(UiMode.Mochi),
                    onSelect = { index ->
                        val target = UiMode.entries[index]
                        if (target != UiMode.Mochi) actions.onSetUiMode(target.value)
                    },
                    leading = { KsuTile("browsers", KsuTileColors.Blue) },
                )
            }
        }
        item(key = "appearance") {
            MochiListSection(header = stringResource(R.string.mochi_section_appearance)) {
                MochiListPickerRow(
                    stringResource(R.string.mochi_style),
                    options = persistentListOf(stringResource(R.string.mochi_style_ios), stringResource(R.string.mochi_style_mochi)),
                    selectedIndex = appearance.style.ordinal,
                    onSelect = { index -> reveal.crossfadeOrChange { appearance.updateStyle(MochiStyle.entries[index]) } },
                    leading = { KsuTile("color-palette", KsuTileColors.Indigo) },
                )
                MochiListPickerRow(
                    stringResource(R.string.mochi_dark_mode),
                    options = modeLabels,
                    selectedIndex = modeIndex,
                    onSelect = { index ->
                        val toDark = when (index) {
                            1 -> false
                            2 -> true
                            else -> systemDark
                        }
                        reveal.revealIfShadeChanges(nowDark, toDark) { actions.onSetThemeMode(index) }
                    },
                    leading = { KsuTile("contrast", KsuTileColors.Gray) },
                )
            }
        }
        if (appearance.style == MochiStyle.Mochi) {
            item(key = "colors") {
                MochiListSection(header = stringResource(R.string.mochi_section_colors)) {
                    MochiListRow(
                        stringResource(R.string.mochi_wallpaper_colors),
                        subtitle = stringResource(R.string.mochi_wallpaper_colors_summary),
                        leading = { KsuTile("image", KsuTileColors.Teal) },
                        trailing = { MochiSwitch(appearance.wallpaper, { appearance.updateWallpaper(it) }) },
                    )
                    MochiListPickerRow(
                        stringResource(R.string.mochi_palette_style),
                        options = paletteLabels,
                        selectedIndex = PaletteChoices.indexOf(appearance.palette).coerceAtLeast(0),
                        onSelect = { appearance.updatePalette(PaletteChoices[it]) },
                        leading = { KsuTile("color-filter", KsuTileColors.Orange) },
                    )
                }
            }
            item(key = "seed") {
                MochiListSection(header = stringResource(R.string.mochi_section_seed), footer = stringResource(R.string.mochi_seed_footer)) {
                    // Pad custom content like a normal row
                    MochiSeedPalette(
                        selected = if (appearance.wallpaper) null else appearance.seed,
                        onSelect = { appearance.updateSeed(it) },
                        modifier = Modifier.padding(MochiListDefaults.RowPadding),
                        style = appearance.palette,
                    )
                }
            }
        }
        item(key = "materials") {
            MochiListSection(header = stringResource(R.string.mochi_section_materials)) {
                MochiListRow(
                    stringResource(R.string.mochi_glass),
                    subtitle = stringResource(R.string.mochi_glass_summary),
                    leading = { KsuTile("water", KsuTileColors.Blue) },
                    trailing = {
                        // Keep the switch on while the confirm dialog is open
                        MochiSwitch(appearance.glass || confirmGlass, { on ->
                            if (on) confirmGlass = true else reveal.crossfadeOrChange { appearance.updateGlass(false) }
                        })
                    },
                )
                MochiListRow(
                    stringResource(R.string.mochi_blur),
                    subtitle = stringResource(R.string.mochi_blur_summary),
                    leading = { KsuTile("layers", KsuTileColors.Gray) },
                    trailing = { MochiSwitch(appearance.blur, { on -> reveal.crossfadeOrChange { appearance.updateBlur(on) } }) },
                )
            }
        }
        item(key = "navigation") {
            MochiListSection(header = stringResource(R.string.mochi_section_navigation)) {
                MochiListRow(
                    stringResource(R.string.settings_enable_predictive_back),
                    subtitle = stringResource(R.string.settings_enable_predictive_back_summary),
                    leading = { KsuTile("arrow-undo", KsuTileColors.Gray) },
                    trailing = { MochiSwitch(uiState.enablePredictiveBack, actions.onSetEnablePredictiveBack) },
                )
                MochiListRow(
                    stringResource(R.string.settings_navigation_badge),
                    subtitle = stringResource(R.string.settings_navigation_badge_summary),
                    leading = { KsuTile("ellipse", KsuTileColors.Red) },
                    trailing = { MochiSwitch(uiState.enableNavigationBadge, actions.onSetEnableNavigationBadge) },
                )
            }
        }
        item(key = "layout") {
            var scale by remember(uiState.pageScale) { mutableFloatStateOf(uiState.pageScale) }
            MochiListSection(header = stringResource(R.string.mochi_section_layout)) {
                MochiListSliderRow(
                    value = scale,
                    onValueChange = { scale = it },
                    valueRange = 0.8f..1.1f,
                    steps = 5,
                    title = stringResource(R.string.settings_page_scale),
                    valueText = "${(scale * 100).toInt()}%",
                    supportingText = stringResource(R.string.settings_page_scale_summary),
                    onValueChangeFinished = { actions.onSetPageScale(scale) },
                )
                MochiListRow(
                    stringResource(R.string.settings_module_description_max_lines),
                    subtitle = stringResource(R.string.settings_module_description_max_lines_summary),
                    trailing = {
                        MochiStepper(
                            value = uiState.moduleDescriptionMaxLines,
                            onValueChange = actions.onSetModuleDescriptionMaxLines,
                            range = 1..5,
                        )
                    },
                )
            }
        }
    }

    MochiDialog(
        visible = confirmGlass,
        onDismissRequest = { confirmGlass = false },
        title = stringResource(R.string.mochi_glass_warning_title),
        message = stringResource(R.string.mochi_glass_warning_message),
        icon = { MochiIonicon("warning", tint = MochiTheme.colorScheme.error, size = 44.dp) },
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(stringResource(R.string.mochi_glass_enable), MochiDialogActionRole.Primary, onClick = {
                reveal.crossfadeOrChange { appearance.updateGlass(true) }
            }),
        ),
    )
}
