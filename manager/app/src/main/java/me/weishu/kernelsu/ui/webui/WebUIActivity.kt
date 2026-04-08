package me.weishu.kernelsu.ui.webui

import android.annotation.SuppressLint
import android.content.SharedPreferences
import android.os.Bundle
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.theme.ThemeController
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator

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
            var appSettings by remember { mutableStateOf(ThemeController.getAppSettings(context)) }
            var uiModeValue by remember { mutableStateOf(prefs.getString("ui_mode", UiMode.DEFAULT_VALUE) ?: UiMode.DEFAULT_VALUE) }
            val uiMode = remember(uiModeValue) {
                UiMode.fromValue(uiModeValue)
            }

            DisposableEffect(prefs) {
                val listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
                    if (key in listOf("color_mode", "key_color", "color_style", "color_spec")) {
                        appSettings = ThemeController.getAppSettings(context)
                    } else if (key == "ui_mode") {
                        uiModeValue = prefs.getString("ui_mode", UiMode.DEFAULT_VALUE) ?: UiMode.DEFAULT_VALUE
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

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun MainContent(activity: ComponentActivity, onFinish: () -> Unit) {
    val moduleId = remember { activity.intent.getStringExtra("id") }
    val webUIState = remember { WebUIState() }

    LaunchedEffect(moduleId) {
        if (moduleId == null) {
            onFinish()
            return@LaunchedEffect
        }
        prepareWebView(activity, moduleId, webUIState)
    }

    DisposableEffect(Unit) {
        onDispose { webUIState.dispose(activity) }
    }

    when (val event = webUIState.uiEvent) {
        is WebUIEvent.Error -> {
            LaunchedEffect(event) {
                Toast.makeText(activity, event.message, Toast.LENGTH_SHORT).show()
                onFinish()
            }
        }

        is WebUIEvent.Close -> {
            LaunchedEffect(event) { onFinish() }
        }

        else -> {}
    }
    val isLoading = webUIState.uiEvent is WebUIEvent.Loading

    Crossfade(targetState = isLoading, animationSpec = tween(300)) { loading ->
        if (loading) {
            LoadingContent()
        } else {
            WebUIScreen(webUIState = webUIState)
        }
    }
}

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun LoadingContent() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                InfiniteProgressIndicator()
            }
        }

        UiMode.Material -> {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(MaterialTheme.colorScheme.background),
                contentAlignment = Alignment.Center
            ) {
                androidx.compose.material3.LoadingIndicator()
            }
        }
    }
}
