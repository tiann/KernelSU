package me.weishu.kernelsu.ui.wear

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.ButtonDefaults
import androidx.wear.compose.material3.Card
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.ui.LocalScreenShape

@Composable
fun wearHorizontalPadding(): Dp = if (LocalScreenShape.current == "round") 10.dp else 0.dp

fun Modifier.wearPaddedFullWidth(horizontalPadding: Dp): Modifier =
    padding(horizontal = horizontalPadding).fillMaxWidth()

@Composable
fun WearPrimaryButton(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    secondaryLabel: String? = null,
    enabled: Boolean = true,
) {
    val colors = ButtonDefaults.buttonColors(
        containerColor = MaterialTheme.colorScheme.primaryDim,
        contentColor = MaterialTheme.colorScheme.onPrimaryContainer,
        secondaryContentColor = MaterialTheme.colorScheme.onPrimaryContainer,
        iconColor = MaterialTheme.colorScheme.onPrimaryContainer,
    )
    if (secondaryLabel == null) {
        Button(
            modifier = modifier,
            enabled = enabled,
            onClick = onClick,
            label = { Text(label, maxLines = 1) },
            colors = colors,
        )
    } else {
        Button(
            modifier = modifier,
            enabled = enabled,
            onClick = onClick,
            label = { Text(label, maxLines = 1) },
            secondaryLabel = { Text(secondaryLabel, maxLines = 1) },
            colors = colors,
        )
    }
}

@Composable
fun WearTonalButton(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    secondaryLabel: String? = null,
    enabled: Boolean = true,
) {
    val colors = ButtonDefaults.filledTonalButtonColors(
        containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
        contentColor = MaterialTheme.colorScheme.onSurface,
        secondaryContentColor = MaterialTheme.colorScheme.onSurfaceVariant,
        iconColor = MaterialTheme.colorScheme.onSurface,
    )
    if (secondaryLabel == null) {
        Button(
            modifier = modifier,
            enabled = enabled,
            onClick = onClick,
            label = { Text(label, maxLines = 1) },
            colors = colors,
        )
    } else {
        Button(
            modifier = modifier,
            enabled = enabled,
            onClick = onClick,
            label = { Text(label, maxLines = 1) },
            secondaryLabel = { Text(secondaryLabel, maxLines = 1) },
            colors = colors,
        )
    }
}

@Composable
fun WearInfoCard(
    title: String,
    body: String? = null,
    modifier: Modifier = Modifier,
) {
    Card(
        onClick = {},
        modifier = modifier,
    ) {
        Text(text = title, style = MaterialTheme.typography.titleSmall)
        if (!body.isNullOrEmpty()) {
            Text(text = body, style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
fun WearMessageText(
    text: String,
    modifier: Modifier = Modifier,
    isError: Boolean = false,
) {
    Text(
        text = text,
        style = MaterialTheme.typography.bodySmall,
        color = if (isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = modifier,
    )
}

@Composable
fun WearSearchField(
    value: String,
    placeholder: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    BasicTextField(
        value = value,
        onValueChange = onValueChange,
        singleLine = true,
        textStyle = MaterialTheme.typography.bodyMedium.copy(
            color = MaterialTheme.colorScheme.onSurface,
        ),
        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
        cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
        modifier = modifier
            .background(
                color = MaterialTheme.colorScheme.surfaceContainerHigh,
                shape = RoundedCornerShape(22.dp),
            )
            .padding(horizontal = 14.dp, vertical = 12.dp),
        decorationBox = { innerTextField ->
            if (value.isEmpty()) {
                Text(
                    text = placeholder,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
            innerTextField()
        },
    )
}

@Composable
fun WearTextField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    readOnly: Boolean = false,
    isError: Boolean = false,
    supportingText: String? = null,
    singleLine: Boolean = true,
) {
    Column(modifier = modifier) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelMedium,
            color = if (isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(horizontal = 4.dp, vertical = 4.dp),
        )
        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            readOnly = readOnly,
            singleLine = singleLine,
            textStyle = MaterialTheme.typography.bodyMedium.copy(
                color = MaterialTheme.colorScheme.onSurface,
            ),
            cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
            modifier = Modifier
                .fillMaxWidth()
                .background(
                    color = MaterialTheme.colorScheme.surfaceContainerHigh,
                    shape = RoundedCornerShape(22.dp),
                )
                .padding(horizontal = 14.dp, vertical = 12.dp),
            decorationBox = { innerTextField ->
                if (value.isEmpty()) {
                    Text(
                        text = label,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
                innerTextField()
            },
        )
        if (!supportingText.isNullOrEmpty()) {
            Text(
                text = supportingText,
                style = MaterialTheme.typography.bodySmall,
                color = if (isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(horizontal = 4.dp, vertical = 4.dp),
            )
        }
    }
}
