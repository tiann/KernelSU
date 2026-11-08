package me.weishu.kernelsu.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialExpressiveTheme
import androidx.compose.material3.MotionScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.core.view.WindowInsetsControllerCompat
import me.weishu.kernelsu.ui.theme.engine.LocalSygiTheme
import me.weishu.kernelsu.ui.theme.engine.SygiThemeManager
import me.weishu.kernelsu.ui.theme.engine.sygiShapes
import me.weishu.kernelsu.ui.theme.engine.withSygi
import me.weishu.kernelsu.ui.theme.engine.withSygiOverrides
import me.weishu.kernelsu.ui.webui.MonetColorsProvider

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

    val colorScheme = rememberKernelSUColorScheme(
        seedColor = if (dynamicColor) Color.Unspecified else Color(appSettings.keyColor),
        isDark = darkTheme,
        isAmoled = amoledMode,
        paletteStyle = appSettings.paletteStyle,
        colorSpec = appSettings.colorSpec,
    )

    LaunchedEffect(darkTheme) {
        val window = (context as? Activity)?.window ?: return@LaunchedEffect
        WindowInsetsControllerCompat(window, window.decorView).apply {
            isAppearanceLightStatusBars = !darkTheme
            isAppearanceLightNavigationBars = !darkTheme
        }
    }

    val animatedColorScheme = colorScheme.animateAsState()
    val pack by SygiThemeManager.active.collectAsState()
    val themedColorScheme = animatedColorScheme.withSygiOverrides(pack, darkTheme, amoledMode)

    MaterialExpressiveTheme(
        colorScheme = themedColorScheme,
        motionScheme = MotionScheme.expressive(),
        shapes = sygiShapes(pack),
        typography = Typography.withSygi(pack),
        content = {
            MonetColorsProvider.UpdateCss(colorScheme)
            CompositionLocalProvider(LocalSygiTheme provides pack) {
                content()
            }
        }
    )
}
