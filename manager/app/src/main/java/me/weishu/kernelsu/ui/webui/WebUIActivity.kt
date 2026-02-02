package me.weishu.kernelsu.ui.webui

import android.annotation.SuppressLint
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator

@SuppressLint("SetJavaScriptEnabled")
class WebUIActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {

        // Enable edge to edge
        enableEdgeToEdge()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isNavigationBarContrastEnforced = false
        }

        super.onCreate(savedInstanceState)

        setContent {
            val prefs = LocalContext.current.getSharedPreferences("settings", MODE_PRIVATE)
            var colorMode by remember { mutableIntStateOf(prefs.getInt("color_mode", 0)) }
            var keyColorInt by remember { mutableIntStateOf(prefs.getInt("key_color", 0)) }
            val keyColor =
                remember(keyColorInt) {
                    if (keyColorInt == 0) null else androidx.compose.ui.graphics.Color(
                        keyColorInt
                    )
                }
            KernelSUTheme(colorMode = colorMode, keyColor = keyColor) {
                MainContent(activity = this, onFinish = { finish() })
            }
        }
    }
}

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
        onDispose { webUIState.dispose() }
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
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                InfiniteProgressIndicator()
            }
        } else {
            WebUIScreen(webUIState = webUIState)
        }
    }
}