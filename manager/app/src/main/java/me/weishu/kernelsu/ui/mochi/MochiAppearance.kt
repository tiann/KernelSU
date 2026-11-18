package me.weishu.kernelsu.ui.mochi

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.edit
import ui.feice.mochi.effect.content.MochiThemeRevealHost
import ui.feice.mochi.effect.content.MochiThemeRevealState
import ui.feice.mochi.effect.content.rememberMochiThemeRevealState
import ui.feice.mochi.effect.overscroll.ProvideMochiOverscroll
import ui.feice.mochi.icons.LocalMochiIconStyle
import ui.feice.mochi.icons.MochiIconStyle
import ui.feice.mochi.ios.iosColorScheme
import ui.feice.mochi.ios.iosControlPalette
import ui.feice.mochi.ios.iosMaterials
import ui.feice.mochi.theme.ColorSchemeMode
import ui.feice.mochi.theme.LocalMochiControlPalette
import ui.feice.mochi.theme.MochiTheme
import ui.feice.mochi.theme.PaletteStyle
import ui.feice.mochi.theme.animateMochiColorScheme
import ui.feice.mochi.theme.defaultTextStyles
import ui.feice.mochi.theme.ThemeController as MochiThemeController

/** Mochi derives Monet colors from a seed or the wallpaper, iOS uses system colors. */
enum class MochiStyle(val value: String) {
    Ios("ios"),
    Mochi("mochi");

    companion object {
        fun from(value: String?): MochiStyle = entries.firstOrNull { it.value == value } ?: Mochi
    }
}

/**
 * Appearance settings for the Mochi UI, shared by all screens and persisted to the settings prefs.
 * Light/dark still comes from the existing theme_mode setting.
 */
class MochiAppearance private constructor(private val prefs: SharedPreferences) {
    var style by mutableStateOf(MochiStyle.from(prefs.getString(KEY_STYLE, null)))
        private set
    var seed by mutableStateOf(Color(prefs.getInt(KEY_SEED, DEFAULT_SEED)))
        private set
    var palette by mutableStateOf(
        PaletteStyle.entries.firstOrNull { it.name == prefs.getString(KEY_PALETTE, null) } ?: PaletteStyle.Neutral,
    )
        private set

    var wallpaper by mutableStateOf(prefs.getBoolean(KEY_WALLPAPER, false))
        private set

    var glass by mutableStateOf(prefs.getBoolean(KEY_GLASS, false))
        private set

    var blur by mutableStateOf(prefs.getBoolean(KEY_BLUR, true))
        private set

    fun updateStyle(value: MochiStyle) {
        style = value
        prefs.edit { putString(KEY_STYLE, value.value) }
    }

    fun updateSeed(value: Color) {
        seed = value
        wallpaper = false
        prefs.edit {
            putInt(KEY_SEED, value.toArgb())
            putBoolean(KEY_WALLPAPER, false)
        }
    }

    fun updatePalette(value: PaletteStyle) {
        palette = value
        prefs.edit { putString(KEY_PALETTE, value.name) }
    }

    fun updateWallpaper(value: Boolean) {
        wallpaper = value
        prefs.edit { putBoolean(KEY_WALLPAPER, value) }
    }

    fun updateGlass(value: Boolean) {
        glass = value
        prefs.edit { putBoolean(KEY_GLASS, value) }
    }

    fun updateBlur(value: Boolean) {
        blur = value
        prefs.edit { putBoolean(KEY_BLUR, value) }
    }

    companion object {
        private const val KEY_STYLE = "mochi_style"
        private const val KEY_SEED = "mochi_seed"
        private const val KEY_PALETTE = "mochi_palette"
        private const val KEY_WALLPAPER = "mochi_wallpaper"
        private const val KEY_GLASS = "mochi_glass"
        private const val KEY_BLUR = "mochi_blur"

        // KernelSU icon green
        private const val DEFAULT_SEED = 0xFF2E7D4F.toInt()

        @Volatile
        private var instance: MochiAppearance? = null

        fun get(context: Context): MochiAppearance = instance ?: synchronized(this) {
            instance ?: MochiAppearance(
                context.applicationContext.getSharedPreferences("settings", Context.MODE_PRIVATE),
            ).also { instance = it }
        }
    }
}

val LocalMochiReveal = staticCompositionLocalOf<MochiThemeRevealState?> { null }

/**
 * Root theme for the Mochi UI. Colors and materials are set up once here.
 */
@Composable
fun MochiKsuTheme(dark: Boolean, content: @Composable () -> Unit) {
    val appearance = MochiAppearance.get(LocalContext.current)
    val reveal = rememberMochiThemeRevealState()
    val revealing = reveal.isRevealing
    val mode = when {
        appearance.wallpaper && dark -> ColorSchemeMode.MonetDark
        appearance.wallpaper -> ColorSchemeMode.MonetLight
        dark -> ColorSchemeMode.Dark
        else -> ColorSchemeMode.Light
    }
    // New controller per parameter set instead of mutating it during composition
    val controller = remember(mode, appearance.seed, appearance.palette) {
        MochiThemeController(mode, appearance.seed, appearance.palette)
    }
    val colors = animateMochiColorScheme(
        when (appearance.style) {
            MochiStyle.Ios -> iosColorScheme(dark)
            MochiStyle.Mochi -> controller.currentColorScheme()
        },
        animate = !revealing,
    )
    val controls = when (appearance.style) {
        MochiStyle.Ios -> iosControlPalette(dark)
        MochiStyle.Mochi -> null
    }
    val materials = remember(appearance.glass, appearance.blur) {
        iosMaterials(glass = appearance.glass, blur = appearance.blur)
    }
    CompositionLocalProvider(
        LocalMochiControlPalette provides controls,
        LocalMochiIconStyle provides MochiIconStyle.Filled,
        LocalMochiReveal provides reveal,
    ) {
        MochiTheme(colors = colors, textStyles = defaultTextStyles(), materials = materials) {
            MochiThemeRevealHost(reveal) {
                ProvideMochiOverscroll(content = content)
            }
        }
    }
}
