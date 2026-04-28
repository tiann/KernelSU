package me.weishu.kernelsu.ui.theme

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.platform.LocalContext
import androidx.wear.compose.material3.ColorScheme
import androidx.wear.compose.material3.MaterialTheme
import com.materialkolor.rememberDynamicColorScheme
import me.weishu.kernelsu.ui.webui.MonetColorsProvider

private val defaultWearColorScheme: ColorScheme = ColorScheme(
    primary = Color(0xFFD8BAFA),
    onPrimary = Color(0xFF3C245A),
    primaryContainer = Color(0xFF543B72),
    onPrimaryContainer = Color(0xFFEEDBFF),
    secondary = Color(0xFFCFC1DA),
    onSecondary = Color(0xFF362D40),
    secondaryContainer = Color(0xFF4D4357),
    onSecondaryContainer = Color(0xFFECDDF7),
    tertiary = Color(0xFFF2B7C0),
    onTertiary = Color(0xFF4B252C),
    tertiaryContainer = Color(0xFF653B42),
    onTertiaryContainer = Color(0xFFFFD9DE),
    error = Color(0xFFFFB4AB),
    onError = Color(0xFF690005),
    errorContainer = Color(0xFF93000A),
    onErrorContainer = Color(0xFFFFDAD6),
    background = Color(0xFF151218),
    onBackground = Color(0xFFE8E0E8),
    onSurface = Color(0xFFE8E0E8),
    onSurfaceVariant = Color(0xFFCCC4CF),
    outline = Color(0xFF958E98),
    outlineVariant = Color(0xFF4A454E),
    surfaceContainerLow = Color(0xFF1D1A20),
    surfaceContainer = Color(0xFF221E24),
    surfaceContainerHigh = Color(0xFF2C292F),
    primaryDim = Color(0xFF543B72),
    secondaryDim = Color(0xFF4D4357),
    tertiaryDim = Color(0xFF653B42),
)

@Composable
fun WearKernelSUTheme(
    appSettings: AppSettings,
    content: @Composable () -> Unit
) {
    val context = LocalContext.current
    val hasCustomColor = appSettings.keyColor != 0

    val wearColorScheme = if (hasCustomColor) {
        // Generate a dark MD3 scheme from the custom seed and map it to Wear tokens.
        val md3 = rememberDynamicColorScheme(
            seedColor = Color(appSettings.keyColor),
            isDark = true,
            isAmoled = true,
            style = appSettings.paletteStyle,
            specVersion = appSettings.colorSpec,
        )
        // Wear button defaults use the *Dim tokens, so darken the accent colors here.
        ColorScheme(
            primary = md3.primary,
            onPrimary = md3.onPrimary,
            primaryContainer = md3.primaryContainer,
            onPrimaryContainer = md3.onPrimaryContainer,
            secondary = md3.secondary,
            onSecondary = md3.onSecondary,
            secondaryContainer = md3.secondaryContainer,
            onSecondaryContainer = md3.onSecondaryContainer,
            tertiary = md3.tertiary,
            onTertiary = md3.onTertiary,
            tertiaryContainer = md3.tertiaryContainer,
            onTertiaryContainer = md3.onTertiaryContainer,
            error = md3.error,
            onError = md3.onError,
            errorContainer = md3.errorContainer,
            onErrorContainer = md3.onErrorContainer,
            background = md3.background,
            onBackground = md3.onBackground,
            onSurface = md3.onSurface,
            onSurfaceVariant = md3.onSurfaceVariant,
            outline = md3.outline,
            outlineVariant = md3.outlineVariant,
            surfaceContainerLow = md3.surfaceContainerLow,
            surfaceContainer = md3.surfaceContainer,
            surfaceContainerHigh = md3.surfaceContainerHigh,
            primaryDim = lerp(md3.primary, Color.Black, 0.6f),
            secondaryDim = lerp(md3.secondary, Color.Black, 0.6f),
            tertiaryDim = lerp(md3.tertiary, Color.Black, 0.6f),
        )
    } else {
        // Keep a stable dark Wear palette; emulator dynamic colors are often too bright.
        defaultWearColorScheme
    }

    MaterialTheme(
        colorScheme = wearColorScheme,
        content = {
            MonetColorsProvider.UpdateCss()
            content()
        }
    )
}
