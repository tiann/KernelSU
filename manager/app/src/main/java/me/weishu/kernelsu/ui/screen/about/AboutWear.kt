package me.weishu.kernelsu.ui.screen.about

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.ButtonDefaults
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.ui.LocalScreenShape

@Composable
fun AboutScreenWear(
    state: AboutUiState,
    actions: AboutScreenActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = if (LocalScreenShape.current == "round") 10.dp else 0.dp

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(state.title)
                }
            }
            item {
                Text(
                    text = state.appName,
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(horizontal = horizontalPadding),
                )
            }
            item {
                Text(
                    text = "${state.versionName}",
                    style = MaterialTheme.typography.bodySmall,
                    modifier = Modifier.padding(horizontal = horizontalPadding),
                )
            }
            state.links.forEach { link ->
                item {
                    Button(
                        modifier = Modifier
                            .padding(horizontal = horizontalPadding)
                            .fillMaxWidth(),
                        onClick = { actions.onOpenLink(link.url) },
                        colors = ButtonDefaults.filledTonalButtonColors(),
                        label = { Text(link.fullText) },
                    )
                }
            }
        }
    }
}
