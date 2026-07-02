package me.weishu.kernelsu.ui.webui

import android.annotation.SuppressLint
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
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
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUILoadState
import me.weishu.kernelsu.ui.webui.ui.WebUILoading
import me.weishu.kernelsu.ui.webui.ui.WebUIScreen
import me.weishu.kernelsu.ui.webui.ui.rememberFileLauncher
import me.weishu.kernelsu.ui.webui.util.setTaskDescription
import me.weishu.kernelsu.ui.webui.viewmodel.WebUIViewModel
import me.weishu.kernelsu.ui.webui.webview.prepareWebView

@SuppressLint("SetJavaScriptEnabled")
class WebUIActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {

        enableEdgeToEdge()
        window.isNavigationBarContrastEnforced = false
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)

        super.onCreate(savedInstanceState)

        setContent {
            val context = LocalContext.current
            val prefs = context.getSharedPreferences("settings", MODE_PRIVATE)
            val settingsRepo = remember { SettingsRepositoryImpl() }
            var appSettings by remember { mutableStateOf(ThemeController.getAppSettings(context)) }
            var uiModeValue by remember { mutableStateOf(settingsRepo.uiMode) }
            val uiMode = remember(uiModeValue) {
                UiMode.fromValue(uiModeValue)
            }

            DisposableEffect(prefs) {
                val listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
                    if (key in listOf("color_mode", "key_color", "color_style", "color_spec")) {
                        appSettings = ThemeController.getAppSettings(context)
                    } else if (key == "ui_mode") {
                        uiModeValue = settingsRepo.uiMode
                    }
                }
                prefs.registerOnSharedPreferenceChangeListener(listener)
                onDispose { prefs.unregisterOnSharedPreferenceChangeListener(listener) }
            }

            CompositionLocalProvider(LocalUiMode provides uiMode) {
                KernelSUTheme(appSettings = appSettings, uiMode = uiMode) {
                    MainContent(activity = this, onFinish = { finish() })
                }
            }
        }
    }
}

@Composable
private fun MainContent(activity: ComponentActivity, onFinish: () -> Unit) {
    val moduleId = remember { activity.intent.data?.getQueryParameter("id") }
    val viewModel = viewModel<WebUIViewModel>()
    val state by viewModel.state.collectAsStateWithLifecycle()
    val fileLauncher = rememberFileLauncher(viewModel::dispatch)

    LaunchedEffect(moduleId) {
        if (moduleId == null) {
            onFinish()
            return@LaunchedEffect
        }
        prepareWebView(
            activity = activity,
            moduleId = moduleId,
            runtime = viewModel.runtime,
            getState = { viewModel.state.value },
            dispatch = viewModel::dispatch,
        )
    }

    LaunchedEffect(Unit) {
        viewModel.effect.collect { effect ->
            when (effect) {
                is WebUIEffect.ShowToast -> Toast.makeText(activity, effect.message, Toast.LENGTH_SHORT).show()
                WebUIEffect.Finish -> onFinish()
                is WebUIEffect.LaunchFileChooser -> {
                    try {
                        fileLauncher.launch(effect.intent)
                    } catch (_: Exception) {
                        viewModel.dispatch(WebUIIntent.FileChooserResult(null))
                    }
                }

                is WebUIEffect.EvaluateJavascript -> viewModel.runtime.webView?.evaluateJavascript(effect.script, null)

                is WebUIEffect.OpenExternalBrowser -> {
                    try {
                        val intent = Intent(Intent.ACTION_VIEW, effect.url.toUri())
                        activity.startActivity(intent)
                    } catch (_: Exception) { }
                }
            }
        }
    }

    DisposableEffect(Unit) {
        onDispose {
            activity.setTaskDescription(activity.getString(R.string.app_name))
            viewModel.runtime.dispose()
        }
    }

    val isLoading = state.loadState == WebUILoadState.Loading

    val webuiContent = @Composable {
        Crossfade(targetState = isLoading, animationSpec = tween(300)) { loading ->
            if (loading) {
                WebUILoading()
            } else {
                WebUIScreen(
                    state = state,
                    runtime = viewModel.runtime,
                    dispatch = viewModel::dispatch,
                )
            }
        }
    }

    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            top.yukonga.miuix.kmp.basic.Surface(modifier = Modifier.fillMaxSize()) {
                webuiContent()
            }
        }

        UiMode.Material -> {
            Surface(modifier = Modifier.fillMaxSize()) {
                webuiContent()
            }
        }
    }
}
