package me.weishu.kernelsu.ui.webui

import android.content.Intent
import android.content.SharedPreferences
import android.content.res.Configuration
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.core.net.toUri
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.theme.ThemeController
import me.weishu.kernelsu.ui.webui.model.WebUIEffect
import me.weishu.kernelsu.ui.webui.ui.WebUIScreen
import me.weishu.kernelsu.ui.webui.ui.rememberFileLauncher
import me.weishu.kernelsu.ui.webui.viewmodel.WebUIViewModel
import me.weishu.kernelsu.ui.webui.webview.prepareWebView

class WebUIActivity : ComponentActivity() {

    private val settingsRepo = SettingsRepositoryImpl()
    private var lastNightMode: Int = Configuration.UI_MODE_NIGHT_UNDEFINED

    private val prefsListener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
        if (key in listOf("color_mode", "miuix_monet", "key_color", "color_style", "color_spec", "ui_mode")) {
            recreate()
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        val appSettings = ThemeController.getAppSettings(settingsRepo)
        if (!appSettings.colorMode.isSystem) return

        val newNightMode = newConfig.uiMode and Configuration.UI_MODE_NIGHT_MASK
        if (newNightMode != lastNightMode) {
            lastNightMode = newNightMode
            recreate()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        window.isNavigationBarContrastEnforced = false

        super.onCreate(savedInstanceState)

        lastNightMode = resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK

        val appSettings = ThemeController.getAppSettings(settingsRepo)
        val uiMode = UiMode.fromValue(settingsRepo.uiMode)

        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        prefs.registerOnSharedPreferenceChangeListener(prefsListener)

        setContent {
            CompositionLocalProvider(LocalUiMode provides uiMode) {
                KernelSUTheme(appSettings = appSettings, uiMode = uiMode) {
                    when (uiMode) {
                        UiMode.Miuix -> {
                            top.yukonga.miuix.kmp.basic.Surface(modifier = Modifier.fillMaxSize()) {
                                MainContent(activity = this@WebUIActivity, onFinish = { finish() })
                            }
                        }

                        UiMode.Material -> {
                            Surface(modifier = Modifier.fillMaxSize()) {
                                MainContent(activity = this@WebUIActivity, onFinish = { finish() })
                            }
                        }
                    }
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        prefs.unregisterOnSharedPreferenceChangeListener(prefsListener)
    }
}

@Composable
private fun MainContent(activity: ComponentActivity, onFinish: () -> Unit) {
    val viewModel = viewModel<WebUIViewModel>()
    val state by viewModel.state.collectAsStateWithLifecycle()
    val runtime = remember { WebUIRuntime() }

    DisposableEffect(runtime) {
        onDispose {
            runtime.dispose()
        }
    }

    val launchFile = rememberFileLauncher(runtime::consumePendingFileCallback)

    LaunchedEffect(Unit) {
        prepareWebView(
            activity = activity,
            runtime = runtime,
            viewModel = viewModel,
        )
    }

    LaunchedEffect(Unit) {
        viewModel.effect.collect { effect ->
            when (effect) {
                is WebUIEffect.ShowToast -> Toast.makeText(activity, effect.message, Toast.LENGTH_SHORT).show()
                WebUIEffect.Finish -> onFinish()
                is WebUIEffect.RequestFileChooser -> {
                    runtime.takeFileChooserIntent(effect.requestId)?.let { intent ->
                        launchFile(effect.requestId, intent)
                    }
                }

                is WebUIEffect.OpenExternalLink -> {
                    try {
                        val intent = Intent(Intent.ACTION_VIEW, effect.url.toUri())
                        activity.startActivity(intent)
                    } catch (_: Exception) {
                        Toast.makeText(activity, activity.getString(R.string.webui_failed_to_open_link, effect.url), Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }
    }

    WebUIScreen(
        state = state,
        runtime = runtime,
        viewModel = viewModel,
    )
}
