package me.weishu.kernelsu.ui.component.markdown

import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
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
import androidx.compose.ui.graphics.graphicsLayer
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
    var loaded by remember(content, isMarkdown) { mutableStateOf(false) }
    val alpha by animateFloatAsState(
        targetValue = if (loaded) 1f else 0f,
        animationSpec = tween(durationMillis = 300),
        label = "MarkdownContentAlpha",
    )
    val placeholderAlpha by animateFloatAsState(
        targetValue = if (loaded) 0f else 1f,
        animationSpec = tween(durationMillis = 150),
        label = "MarkdownContentPlaceholderAlpha",
    )
    val containerColor = when (LocalUiMode.current) {
        UiMode.Material -> MaterialTheme.colorScheme.surfaceContainerHigh
        UiMode.Miuix -> null
    }
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .animateContentSize(animationSpec = tween(durationMillis = 300))
    ) {
        Box(
            modifier = Modifier
                .verticalScroll(rememberScrollState())
                .graphicsLayer { this.alpha = alpha }
        ) {
            GithubMarkdown(
                content = content,
                isMarkdown = isMarkdown,
                onLoadingChange = { loaded = !it },
                containerColor = containerColor,
            )
        }
        if (placeholderAlpha > 0f) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 64.dp)
                    .graphicsLayer { this.alpha = placeholderAlpha },
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
