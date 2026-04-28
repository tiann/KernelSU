package me.weishu.kernelsu.ui

import android.content.pm.PackageManager
import androidx.compose.runtime.staticCompositionLocalOf
import me.weishu.kernelsu.ksuApp

enum class UiMode(val value: String) {
    Miuix("miuix"),
    Material("material"),
    Wear("wear");

    companion object {
        fun fromValue(value: String): UiMode = when (value) {
            Material.value -> Material
            Wear.value -> Wear
            else -> Miuix
        }

        val isWatchDevice: Boolean by lazy {
            ksuApp.packageManager.hasSystemFeature(PackageManager.FEATURE_WATCH)
        }

        val DEFAULT_VALUE: String
            get() = if (isWatchDevice) Wear.value else Miuix.value

        val defaultUiMode: UiMode
            get() = if (isWatchDevice) Wear else Miuix
    }
}

val LocalUiMode = staticCompositionLocalOf { UiMode.defaultUiMode }

val LocalScreenShape = staticCompositionLocalOf { "round" }
