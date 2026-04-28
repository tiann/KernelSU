package me.weishu.kernelsu.ui.screen.colorpalette

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Card
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalScreenShape

@Composable
fun ColorPaletteScreenWear(
    state: ColorPaletteUiState,
    actions: ColorPaletteScreenActions,
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
                    Text(stringResource(R.string.settings_theme))
                }
            }
            item {
                Card(
                    onClick = {},
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                ) {
                    Text(
                        text = stringResource(R.string.settings_theme_wear_note),
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
        }
    }
}
