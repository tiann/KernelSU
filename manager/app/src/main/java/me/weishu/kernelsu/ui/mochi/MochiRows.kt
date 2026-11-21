package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.core.graphics.ColorUtils
import me.weishu.kernelsu.R
import ui.feice.mochi.basic.MochiSwitch
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.shape.MochiRoundedRectangle
import ui.feice.mochi.theme.MochiTheme

private val IosTileShape = MochiRoundedRectangle(7.dp)

private val MochiTileShape = MochiRoundedRectangle(9.5.dp)

/**
 * Category colors for leading tiles. The iOS style uses them directly,
 * the Mochi style derives the real color from the theme primary (see [mochiTileTone]).
 */
object KsuTileColors {
    val Gray = Color(0xFF8E8E93)
    val Blue = Color(0xFF0A84FF)
    val Green = Color(0xFF30B158)
    val Orange = Color(0xFFFF9500)
    val Red = Color(0xFFFF3B30)
    val Indigo = Color(0xFF5E5CE6)
    val Teal = Color(0xFF30B0C7)
}

class KsuTone(val container: Color, val content: Color)

/**
 * Tile colors for the Mochi style: each category shifts the primary hue a little,
 * light container with dark content, reversed in dark mode. Red follows the error color.
 */
private fun mochiTileTone(category: Color, primary: Color, error: Color, dark: Boolean): KsuTone {
    val base = FloatArray(3).also { ColorUtils.colorToHSL((if (category == KsuTileColors.Red) error else primary).toArgb(), it) }
    val (shift, sat) = when (category) {
        KsuTileColors.Blue -> 0f to 1f
        KsuTileColors.Indigo -> 26f to 0.9f
        KsuTileColors.Teal -> -26f to 0.9f
        KsuTileColors.Green -> -48f to 0.8f
        KsuTileColors.Orange -> 48f to 0.85f
        KsuTileColors.Red -> 0f to 1f
        else -> 0f to 0.18f
    }
    val hue = ((base[0] + shift) % 360f + 360f) % 360f
    // Keep some saturation even when the primary is almost gray
    val s = (base[1].coerceIn(0.35f, 0.75f) * sat).coerceIn(0.06f, 0.7f)
    fun hsl(sat: Float, light: Float) = Color(ColorUtils.HSLToColor(floatArrayOf(hue, sat.coerceIn(0f, 1f), light)))
    return if (dark) {
        KsuTone(container = hsl(s * 0.55f, 0.26f), content = hsl(s * 1.1f, 0.80f))
    } else {
        KsuTone(container = hsl(s * 0.9f, 0.89f), content = hsl(s * 1.15f, 0.36f))
    }
}

@Composable
fun rememberKsuTone(category: Color = KsuTileColors.Blue): KsuTone {
    val scheme = MochiTheme.colorScheme
    return remember(category, scheme.primary, scheme.error, scheme.isDark) {
        mochiTileTone(category, scheme.primary, scheme.error, scheme.isDark)
    }
}

/** 28dp leading tile with a filled icon. */
@Composable
fun KsuTile(icon: String, color: Color = KsuTileColors.Gray, enabled: Boolean = true) {
    val appearance = MochiAppearance.get(LocalContext.current)
    val scheme = MochiTheme.colorScheme
    val mochi = appearance.style == MochiStyle.Mochi
    val tone = if (mochi) {
        remember(color, scheme.primary, scheme.error, scheme.isDark) { mochiTileTone(color, scheme.primary, scheme.error, scheme.isDark) }
    } else {
        null
    }
    val container = tone?.container ?: color
    val content = tone?.content ?: Color.White
    Box(
        Modifier
            .size(28.dp)
            .background(if (enabled) container else container.copy(alpha = 0.4f), if (mochi) MochiTileShape else IosTileShape),
        contentAlignment = Alignment.Center,
    ) {
        MochiIonicon(icon, tint = if (enabled) content else content.copy(alpha = 0.6f), size = if (mochi) 16.dp else 17.dp)
    }
}

/** Leading tile with the mochi dango mark, same size and shape as [KsuTile]. */
@Composable
fun MochiLogoTile() {
    val mochi = MochiAppearance.get(LocalContext.current).style == MochiStyle.Mochi
    val scheme = MochiTheme.colorScheme
    Box(
        Modifier
            .size(28.dp)
            .background(scheme.surfaceContainerHighest, if (mochi) MochiTileShape else IosTileShape),
        contentAlignment = Alignment.Center,
    ) {
        Image(painterResource(R.drawable.ic_mochi_dango), contentDescription = null, modifier = Modifier.size(22.dp))
    }
}

@Composable
fun KsuSwitchRow(
    title: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    icon: String? = null,
    color: Color = KsuTileColors.Gray,
    subtitle: String? = null,
    enabled: Boolean = true,
) {
    MochiListRow(
        title,
        subtitle = subtitle,
        enabled = enabled,
        leading = icon?.let { { KsuTile(it, color, enabled) } },
        trailing = { MochiSwitch(checked, onCheckedChange, enabled = enabled) },
    )
}
