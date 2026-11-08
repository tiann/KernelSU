package me.weishu.kernelsu.ui.theme.engine

import android.graphics.BitmapFactory
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.Shapes
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.rememberVectorPainter
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.max
import kotlin.math.min

/** The active theme pack, or null for the built-in SygiSU look. */
val LocalSygiTheme = staticCompositionLocalOf<SygiThemePack?> { null }

/** A dimension from theme/dimens.xml (for example `nav_corner`), or [default]. */
@Composable
fun sygiDp(name: String, default: Dp): Dp {
    val v = LocalSygiTheme.current?.dimens?.get(name) ?: return default
    return v.dp
}

/** An integer from the theme (for example `icons_use_original_colors`), or [default]. */
@Composable
fun sygiInt(name: String, default: Int): Int =
    LocalSygiTheme.current?.integers?.get(name) ?: default

/** icons/<id>.png from the theme when present, otherwise [fallback]. */
@Composable
fun sygiIconPainter(id: String, fallback: ImageVector): Painter {
    val pack = LocalSygiTheme.current
    val vector = rememberVectorPainter(fallback)
    val custom = remember(pack, id) {
        pack?.iconFile(id)?.let { file ->
            runCatching {
                BitmapFactory.decodeFile(file.absolutePath)?.asImageBitmap()?.let { BitmapPainter(it) }
            }.getOrNull()
        }
    }
    return custom ?: vector
}

// ---------- colors ----------

private const val MIN_CONTRAST = 3f

private fun contrast(a: Color, b: Color): Float {
    val la = a.luminance()
    val lb = b.luminance()
    return (max(la, lb) + 0.05f) / (min(la, lb) + 0.05f)
}

/**
 * Applies theme colors on top of the generated Material scheme. Role names match Material 3
 * (primary, onPrimary, surfaceContainer, ...). Unreadable pairs are reverted so a theme cannot
 * hide text or warnings.
 */
fun ColorScheme.withSygiOverrides(pack: SygiThemePack?, dark: Boolean, amoled: Boolean): ColorScheme {
    val map = pack?.resolveColors(dark, amoled) ?: return this
    if (map.isEmpty()) return this
    var s = this
    for ((key, value) in map) {
        val c = Color(value)
        s = when (key) {
            "primary" -> s.copy(primary = c)
            "onPrimary" -> s.copy(onPrimary = c)
            "primaryContainer" -> s.copy(primaryContainer = c)
            "onPrimaryContainer" -> s.copy(onPrimaryContainer = c)
            "inversePrimary" -> s.copy(inversePrimary = c)
            "secondary" -> s.copy(secondary = c)
            "onSecondary" -> s.copy(onSecondary = c)
            "secondaryContainer" -> s.copy(secondaryContainer = c)
            "onSecondaryContainer" -> s.copy(onSecondaryContainer = c)
            "tertiary" -> s.copy(tertiary = c)
            "onTertiary" -> s.copy(onTertiary = c)
            "tertiaryContainer" -> s.copy(tertiaryContainer = c)
            "onTertiaryContainer" -> s.copy(onTertiaryContainer = c)
            "background" -> s.copy(background = c)
            "onBackground" -> s.copy(onBackground = c)
            "surface" -> s.copy(surface = c)
            "onSurface" -> s.copy(onSurface = c)
            "surfaceVariant" -> s.copy(surfaceVariant = c)
            "onSurfaceVariant" -> s.copy(onSurfaceVariant = c)
            "surfaceTint" -> s.copy(surfaceTint = c)
            "inverseSurface" -> s.copy(inverseSurface = c)
            "inverseOnSurface" -> s.copy(inverseOnSurface = c)
            "error" -> s.copy(error = c)
            "onError" -> s.copy(onError = c)
            "errorContainer" -> s.copy(errorContainer = c)
            "onErrorContainer" -> s.copy(onErrorContainer = c)
            "outline" -> s.copy(outline = c)
            "outlineVariant" -> s.copy(outlineVariant = c)
            "scrim" -> s.copy(scrim = c)
            "surfaceBright" -> s.copy(surfaceBright = c)
            "surfaceDim" -> s.copy(surfaceDim = c)
            "surfaceContainer" -> s.copy(surfaceContainer = c)
            "surfaceContainerHigh" -> s.copy(surfaceContainerHigh = c)
            "surfaceContainerHighest" -> s.copy(surfaceContainerHighest = c)
            "surfaceContainerLow" -> s.copy(surfaceContainerLow = c)
            "surfaceContainerLowest" -> s.copy(surfaceContainerLowest = c)
            else -> s
        }
    }
    return s.guardContrast(this)
}

private fun ColorScheme.guardContrast(base: ColorScheme): ColorScheme {
    var s = this
    if (contrast(s.primary, s.onPrimary) < MIN_CONTRAST) s = s.copy(primary = base.primary, onPrimary = base.onPrimary)
    if (contrast(s.secondary, s.onSecondary) < MIN_CONTRAST) s = s.copy(secondary = base.secondary, onSecondary = base.onSecondary)
    if (contrast(s.tertiary, s.onTertiary) < MIN_CONTRAST) s = s.copy(tertiary = base.tertiary, onTertiary = base.onTertiary)
    if (contrast(s.error, s.onError) < MIN_CONTRAST) s = s.copy(error = base.error, onError = base.onError)
    if (contrast(s.primaryContainer, s.onPrimaryContainer) < MIN_CONTRAST) {
        s = s.copy(primaryContainer = base.primaryContainer, onPrimaryContainer = base.onPrimaryContainer)
    }
    if (contrast(s.secondaryContainer, s.onSecondaryContainer) < MIN_CONTRAST) {
        s = s.copy(secondaryContainer = base.secondaryContainer, onSecondaryContainer = base.onSecondaryContainer)
    }
    if (contrast(s.tertiaryContainer, s.onTertiaryContainer) < MIN_CONTRAST) {
        s = s.copy(tertiaryContainer = base.tertiaryContainer, onTertiaryContainer = base.onTertiaryContainer)
    }
    if (contrast(s.errorContainer, s.onErrorContainer) < MIN_CONTRAST) {
        s = s.copy(errorContainer = base.errorContainer, onErrorContainer = base.onErrorContainer)
    }
    if (contrast(s.surface, s.onSurface) < MIN_CONTRAST) s = s.copy(surface = base.surface, onSurface = base.onSurface)
    if (contrast(s.background, s.onBackground) < MIN_CONTRAST) s = s.copy(background = base.background, onBackground = base.onBackground)
    return s
}

// ---------- shapes ----------

/** Built-in SygiSU shapes (big, soft corners), overridable per key from theme/shapes.xml. */
fun sygiShapes(pack: SygiThemePack?): Shapes {
    fun c(name: String, default: Float) = RoundedCornerShape((pack?.corners?.get(name) ?: default).dp)
    return Shapes(
        extraSmall = c("extraSmall", 8f),
        small = c("small", 12f),
        medium = c("medium", 18f),
        large = c("large", 26f),
        extraLarge = c("extraLarge", 34f),
    )
}

// ---------- typography ----------

/** Applies fonts and per-role text styles from the theme. Role names match Material 3. */
fun androidx.compose.material3.Typography.withSygi(pack: SygiThemePack?): androidx.compose.material3.Typography {
    if (pack == null || (pack.fonts.isEmpty() && pack.textStyles.isEmpty())) return this

    fun family(name: String?): FontFamily? {
        val file = name?.let { pack.fonts[it] } ?: return null
        return runCatching { FontFamily(Font(file)) }.getOrNull()
    }

    val defaultFamily = family("default")

    fun style(role: String, base: TextStyle): TextStyle {
        var t = base
        defaultFamily?.let { t = t.copy(fontFamily = it) }
        pack.textStyles[role]?.let { spec ->
            family(spec.font)?.let { t = t.copy(fontFamily = it) }
            spec.sizeSp?.let { t = t.copy(fontSize = it.sp, lineHeight = TextUnit.Unspecified) }
            spec.weight?.let { t = t.copy(fontWeight = FontWeight(it.coerceIn(1, 1000))) }
        }
        return t
    }

    return copy(
        displayLarge = style("displayLarge", displayLarge),
        displayMedium = style("displayMedium", displayMedium),
        displaySmall = style("displaySmall", displaySmall),
        headlineLarge = style("headlineLarge", headlineLarge),
        headlineMedium = style("headlineMedium", headlineMedium),
        headlineSmall = style("headlineSmall", headlineSmall),
        titleLarge = style("titleLarge", titleLarge),
        titleMedium = style("titleMedium", titleMedium),
        titleSmall = style("titleSmall", titleSmall),
        bodyLarge = style("bodyLarge", bodyLarge),
        bodyMedium = style("bodyMedium", bodyMedium),
        bodySmall = style("bodySmall", bodySmall),
        labelLarge = style("labelLarge", labelLarge),
        labelMedium = style("labelMedium", labelMedium),
        labelSmall = style("labelSmall", labelSmall),
    )
}
