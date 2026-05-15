package me.weishu.kernelsu.ui.component.markdown

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.LoadingIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun MarkdownContent(
    content: String,
    isMarkdown: Boolean,
) {
    var isLoading by remember { mutableStateOf(true) }
    val containerColor = when (LocalUiMode.current) {
        UiMode.Material -> MaterialTheme.colorScheme.surfaceContainerHigh
        UiMode.Miuix -> null
    }
    Box(Modifier.fillMaxWidth()) {
        Box(Modifier.verticalScroll(rememberScrollState())) {
            GithubMarkdown(
                content = content,
                isMarkdown = isMarkdown,
                onLoadingChange = { isLoading = it },
                containerColor = containerColor,
            )
        }
        if (isLoading) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 64.dp),
                contentAlignment = Alignment.Center,
            ) {
                when (LocalUiMode.current) {
                    UiMode.Material -> LoadingIndicator()
                    UiMode.Miuix -> InfiniteProgressIndicator()
                }
            }
        }
    }
}
