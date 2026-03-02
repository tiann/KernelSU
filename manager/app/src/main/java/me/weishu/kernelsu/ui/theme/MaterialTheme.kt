package me.weishu.kernelsu.ui.theme

import android.app.Activity
import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.MaterialExpressiveTheme
import androidx.compose.material3.MotionScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.expressiveLightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.core.view.WindowInsetsControllerCompat
import com.materialkolor.rememberDynamicColorScheme
import me.weishu.kernelsu.ui.webui.MonetColorsProvider

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun MaterialKernelSUTheme(
    appSettings: AppSettings,
    content: @Composable () -> Unit
) {
    val context = LocalContext.current
    val systemDarkTheme = isSystemInDarkTheme()
    val darkTheme = appSettings.colorMode.isDark || (appSettings.colorMode.isSystem && systemDarkTheme)
    val amoledMode = appSettings.colorMode.isAmoled
    val dynamicColor = appSettings.keyColor == 0
    val colorStyle = appSettings.paletteStyle
    val colorSpec = appSettings.colorSpec

    val colorScheme = when {
        dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S -> {
            val baseScheme = if (darkTheme) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
            rememberDynamicColorScheme(
                seedColor = Color.Unspecified,
                isDark = darkTheme,
                isAmoled = amoledMode,
                style = colorStyle,
                specVersion = colorSpec,
                primary = baseScheme.primary,
                secondary = baseScheme.secondary,
                tertiary = baseScheme.tertiary,
                neutral = baseScheme.surface,
                neutralVariant = baseScheme.surfaceVariant,
                error = baseScheme.error
            )
        }
        !dynamicColor -> {
            rememberDynamicColorScheme(
                seedColor = Color(appSettings.keyColor),
                isDark = darkTheme,
                isAmoled = amoledMode,
                style = colorStyle,
                specVersion = colorSpec,
            )
        }
        else -> {
            if (darkTheme) darkColorScheme() else expressiveLightColorScheme()
        }
    }

    LaunchedEffect(darkTheme) {
        val window = (context as? Activity)?.window ?: return@LaunchedEffect
        WindowInsetsControllerCompat(window, window.decorView).apply {
            isAppearanceLightStatusBars = !darkTheme
            isAppearanceLightNavigationBars = !darkTheme
        }
    }

    MaterialExpressiveTheme(
        colorScheme = colorScheme,
        motionScheme = MotionScheme.expressive(),
        content = {
            MonetColorsProvider.UpdateCss()
            content()
        }
    )
}
