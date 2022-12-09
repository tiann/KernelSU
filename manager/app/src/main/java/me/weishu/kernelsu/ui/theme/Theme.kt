package me.weishu.kernelsu.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.MaterialTheme
import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val DarkColorPalette = darkColors(
    primary = YELLOW,
    primaryVariant = YELLOW_DARK,
    secondary = SECONDARY_DARK
)

private val LightColorPalette = lightColors(
    primary = YELLOW,
    primaryVariant = YELLOW_LIGHT,
    secondary = SECONDARY_LIGHT
)

@Composable
fun KernelSUTheme(darkTheme: Boolean = isSystemInDarkTheme(), content: @Composable () -> Unit) {
    val colors = if (darkTheme) {
        DarkColorPalette
    } else {
        LightColorPalette
    }

    MaterialTheme(
        colors = colors,
        typography = Typography,
        shapes = Shapes,
        content = content
    )
}