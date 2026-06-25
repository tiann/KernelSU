package me.weishu.kernelsu.ui.webui.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.LoadingIndicator
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator

@Composable
internal fun WebUILoading() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                InfiniteProgressIndicator()
            }
        }

        UiMode.Material -> {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                LoadingIndicator()
            }
        }
    }
}
