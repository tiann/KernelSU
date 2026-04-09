package me.weishu.kernelsu.ui.viewmodel

import android.content.Context
import android.content.SharedPreferences
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import me.weishu.kernelsu.data.repository.SettingsRepository
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.theme.ThemeController

class MainActivityViewModel : ViewModel() {

    private val prefs = ksuApp.getSharedPreferences("settings", Context.MODE_PRIVATE)
    private val settingRepo: SettingsRepository = SettingsRepositoryImpl()
    private val listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
        if (key == null || key in observedKeys) {
            _uiState.value = readUiState()
        }
    }

    private val _uiState = MutableStateFlow(readUiState())
    val uiState: StateFlow<MainActivityUiState> = _uiState.asStateFlow()

    init {
        prefs.registerOnSharedPreferenceChangeListener(listener)
    }

    override fun onCleared() {
        prefs.unregisterOnSharedPreferenceChangeListener(listener)
        super.onCleared()
    }

    private fun readUiState(): MainActivityUiState {
        return MainActivityUiState(
            appSettings = ThemeController.getAppSettings(ksuApp),
            pageScale = settingRepo.pageScale,
            enableBlur = settingRepo.enableBlur,
            enableFloatingBottomBar = settingRepo.enableFloatingBottomBar,
            enableFloatingBottomBarBlur = settingRepo.enableFloatingBottomBarBlur,
            enableSmoothCorner = settingRepo.enableSmoothCorner,
            uiMode = UiMode.fromValue(settingRepo.uiMode),
        )
    }

    private companion object {
        val observedKeys = setOf(
            "color_mode",
            "key_color",
            "color_style",
            "color_spec",
            "page_scale",
            "enable_blur",
            "enable_floating_bottom_bar",
            "enable_floating_bottom_bar_blur",
            "enable_smooth_corner",
            "ui_mode",
        )
    }
}
