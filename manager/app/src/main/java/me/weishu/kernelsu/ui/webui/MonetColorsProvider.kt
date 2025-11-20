package me.weishu.kernelsu.ui.webui

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import top.yukonga.miuix.kmp.theme.MiuixTheme

/**
 * @author rifsxd
 * @date 2025/6/2.
 */
object MonetColorsProvider {

    private var colorsCss: String? = null

    fun getColorsCss(): String {
        return colorsCss ?: ""
    }

    @Composable
    fun UpdateCss() {
        val colorScheme = MiuixTheme.colorScheme

        val monetColors = mapOf(
            // App Base Colors
            "primary" to colorScheme.primary.toCssValue(),
            "onPrimary" to colorScheme.onPrimary.toCssValue(),
            "primaryContainer" to colorScheme.primaryContainer.toCssValue(),
            "onPrimaryContainer" to colorScheme.onPrimaryContainer.toCssValue(),
            "inversePrimary" to colorScheme.inversePrimary.toCssValue(),
            "secondary" to colorScheme.secondary.toCssValue(),
            "onSecondary" to colorScheme.onSecondary.toCssValue(),
            "secondaryContainer" to colorScheme.secondaryContainer.toCssValue(),
            "onSecondaryContainer" to colorScheme.onSecondaryContainer.toCssValue(),
            "tertiary" to colorScheme.tertiary.toCssValue(),
            "onTertiary" to colorScheme.onTertiary.toCssValue(),
            "tertiaryContainer" to colorScheme.tertiaryContainer.toCssValue(),
            "onTertiaryContainer" to colorScheme.onTertiaryContainer.toCssValue(),
            "background" to colorScheme.background.toCssValue(),
            "onBackground" to colorScheme.onBackground.toCssValue(),
            "surface" to colorScheme.surface.toCssValue(),
            "tonalSurface" to colorScheme.surfaceColorAtElevation(1.dp).toCssValue(), // surfaceColorAtElevation(1.dp) not available here
            "onSurface" to colorScheme.onSurface.toCssValue(),
            "surfaceVariant" to colorScheme.surfaceVariant.toCssValue(),
            "onSurfaceVariant" to colorScheme.onSurfaceVariant.toCssValue(),
            "surfaceTint" to colorScheme.surfaceTint.toCssValue(),
            "inverseSurface" to colorScheme.inverseSurface.toCssValue(),
            "inverseOnSurface" to colorScheme.inverseOnSurface.toCssValue(),
            "error" to colorScheme.error.toCssValue(),
            "onError" to colorScheme.onError.toCssValue(),
            "errorContainer" to colorScheme.errorContainer.toCssValue(),
            "onErrorContainer" to colorScheme.onErrorContainer.toCssValue(),
            "outline" to colorScheme.outline.toCssValue(),
            "outlineVariant" to colorScheme.outlineVariant.toCssValue(),
            "scrim" to colorScheme.scrim.toCssValue(),
            "surfaceBright" to colorScheme.surfaceBright.toCssValue(),
            "surfaceDim" to colorScheme.surfaceDim.toCssValue(),
            "surfaceContainer" to colorScheme.surfaceContainer.toCssValue(),
            "surfaceContainerHigh" to colorScheme.surfaceContainerHigh.toCssValue(),
            "surfaceContainerHighest" to colorScheme.surfaceContainerHighest.toCssValue(),
            "surfaceContainerLow" to colorScheme.surfaceContainerLow.toCssValue(),
            "surfaceContainerLowest" to colorScheme.surfaceContainerLowest.toCssValue(),
            "filledTonalButtonContentColor" to colorScheme.onPrimaryContainer.toCssValue(),
            "filledTonalButtonContainerColor" to colorScheme.secondaryContainer.toCssValue(),
            "filledTonalButtonDisabledContentColor" to colorScheme.onSurfaceVariant.toCssValue(),
            "filledTonalButtonDisabledContainerColor" to colorScheme.surfaceVariant.toCssValue(),
            "filledCardContentColor" to colorScheme.onPrimaryContainer.toCssValue(),
            "filledCardContainerColor" to colorScheme.primaryContainer.toCssValue(),
            "filledCardDisabledContentColor" to colorScheme.onSurfaceVariant.toCssValue(),
            "filledCardDisabledContainerColor" to colorScheme.surfaceVariant.toCssValue()
        )

        colorsCss = monetColors.toCssVars()
    }

    private fun Map<String, String>.toCssVars(): String {
        return buildString {
            append(":root {\n")
            for ((k, v) in this@toCssVars) {
                append("  --$k: $v;\n")
            }
            append("}\n")
        }
    }

    private fun Color.toCssValue(): String {
        fun Float.toHex(): String {
            return (this * 255).toInt().coerceIn(0, 255).toString(16).padStart(2, '0')
        }
        return "#${red.toHex()}${green.toHex()}${blue.toHex()}${alpha.toHex()}"
    }
}
