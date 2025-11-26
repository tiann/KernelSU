package me.weishu.kernelsu.ui.webui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.graphics.Color
import top.yukonga.miuix.kmp.theme.MiuixTheme
import java.util.concurrent.atomic.AtomicReference

/**
 * @author rifsxd
 * @date 2025/6/2.
 */
object MonetColorsProvider {

    private val colorsCss: AtomicReference<String?> = AtomicReference(null)

    fun getColorsCss(): String {
        return colorsCss.get() ?: ""
    }

    @Composable
    fun UpdateCss() {
        val colorScheme = MiuixTheme.colorScheme

        LaunchedEffect(colorScheme) {
            // Generate CSS only when colorScheme changes
            val monetColors = mapOf(
                // App Base Colors
                "primary" to colorScheme.primary.toCssValue(),
                "onPrimary" to colorScheme.onPrimary.toCssValue(),
                "primaryContainer" to colorScheme.primaryContainer.toCssValue(),
                "onPrimaryContainer" to colorScheme.onPrimaryContainer.toCssValue(),
                "inversePrimary" to colorScheme.primaryVariant.toCssValue(),
                "secondary" to colorScheme.secondary.toCssValue(),
                "onSecondary" to colorScheme.onSecondary.toCssValue(),
                "secondaryContainer" to colorScheme.secondaryContainer.toCssValue(),
                "onSecondaryContainer" to colorScheme.onSecondaryContainer.toCssValue(),
                "tertiary" to colorScheme.tertiaryContainerVariant.toCssValue(),
                "onTertiary" to colorScheme.tertiaryContainer.toCssValue(),
                "tertiaryContainer" to colorScheme.tertiaryContainer.toCssValue(),
                "onTertiaryContainer" to colorScheme.onTertiaryContainer.toCssValue(),
                "background" to colorScheme.background.toCssValue(),
                "onBackground" to colorScheme.onBackground.toCssValue(),
                "surface" to colorScheme.surface.toCssValue(),
                "tonalSurface" to colorScheme.surfaceContainer.toCssValue(),
                "onSurface" to colorScheme.onSurface.toCssValue(),
                "surfaceVariant" to colorScheme.surfaceVariant.toCssValue(),
                "onSurfaceVariant" to colorScheme.onSurfaceVariantSummary.toCssValue(),
                "surfaceTint" to colorScheme.surface.toCssValue(),
                "inverseSurface" to colorScheme.disabledOnSurface.toCssValue(),
                "inverseOnSurface" to colorScheme.surfaceContainer.toCssValue(),
                "error" to colorScheme.error.toCssValue(),
                "onError" to colorScheme.onError.toCssValue(),
                "errorContainer" to colorScheme.errorContainer.toCssValue(),
                "onErrorContainer" to colorScheme.onErrorContainer.toCssValue(),
                "outline" to colorScheme.outline.toCssValue(),
                "outlineVariant" to colorScheme.dividerLine.toCssValue(),
                "scrim" to colorScheme.windowDimming.toCssValue(),
                "surfaceBright" to colorScheme.surface.toCssValue(),
                "surfaceDim" to colorScheme.surface.toCssValue(),
                "surfaceContainer" to colorScheme.surfaceContainer.toCssValue(),
                "surfaceContainerHigh" to colorScheme.surfaceContainerHigh.toCssValue(),
                "surfaceContainerHighest" to colorScheme.surfaceContainerHighest.toCssValue(),
                "surfaceContainerLow" to colorScheme.surfaceContainer.toCssValue(),
                "surfaceContainerLowest" to colorScheme.surfaceContainer.toCssValue(),
                "filledTonalButtonContentColor" to colorScheme.onPrimaryContainer.toCssValue(),
                "filledTonalButtonContainerColor" to colorScheme.secondaryContainer.toCssValue(),
                "filledTonalButtonDisabledContentColor" to colorScheme.onSurfaceVariantSummary.toCssValue(),
                "filledTonalButtonDisabledContainerColor" to colorScheme.surfaceVariant.toCssValue(),
                "filledCardContentColor" to colorScheme.onPrimaryContainer.toCssValue(),
                "filledCardContainerColor" to colorScheme.primaryContainer.toCssValue(),
                "filledCardDisabledContentColor" to colorScheme.onSurfaceVariantSummary.toCssValue(),
                "filledCardDisabledContainerColor" to colorScheme.surfaceVariant.toCssValue()
            )

            colorsCss.set(monetColors.toCssVars())
        }
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
        return if (alpha == 1f) {
            "#${red.toHex()}${green.toHex()}${blue.toHex()}"
        } else {
            "#${red.toHex()}${green.toHex()}${blue.toHex()}${alpha.toHex()}"
        }
    }
}
