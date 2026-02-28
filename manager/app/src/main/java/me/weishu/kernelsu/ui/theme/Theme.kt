package me.weishu.kernelsu.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import me.weishu.kernelsu.ui.webui.MonetColorsProvider.UpdateCss
import top.yukonga.miuix.kmp.theme.ColorSchemeMode
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.ThemeController

@Composable
fun KernelSUTheme(
    colorMode: Int = 0,
    keyColor: Color? = null,
    content: @Composable () -> Unit
) {
    val isDark = isSystemInDarkTheme()
    val controller = when (colorMode) {
        1 -> ThemeController(ColorSchemeMode.Light)
        2 -> ThemeController(ColorSchemeMode.Dark)
        3 -> ThemeController(
            ColorSchemeMode.MonetSystem,
            keyColor = keyColor,
            isDark = isDark
        )

        4 -> ThemeController(
            ColorSchemeMode.MonetLight,
            keyColor = keyColor,
        )

        5 -> ThemeController(
            ColorSchemeMode.MonetDark,
            keyColor = keyColor,
        )

        else -> ThemeController(ColorSchemeMode.System)
    }
    return MiuixTheme(
        controller = controller,
        content = {
            UpdateCss()
            content()
        }
    )
}

@Composable
@ReadOnlyComposable
fun isInDarkTheme(): Boolean {
    return when (LocalColorMode.current) {
        1, 4 -> false  // Force light mode
        2, 5 -> true   // Force dark mode
        else -> isSystemInDarkTheme()  // Follow system (0 or default)
    }
}


val LocalColorMode = staticCompositionLocalOf { 0 }

val LocalEnableBlur = staticCompositionLocalOf { true }

val LocalEnableFloatingBottomBar = staticCompositionLocalOf { false }

val LocalEnableFloatingBottomBarBlur = staticCompositionLocalOf { true }