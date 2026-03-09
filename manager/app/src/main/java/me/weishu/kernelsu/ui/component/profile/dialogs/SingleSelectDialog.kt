package me.weishu.kernelsu.ui.component.profile.dialogs

import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedRadioItem

@Composable
fun <T> SingleSelectDialog(
    title: String,
    items: List<T>,
    selectedItem: T,
    itemTitle: (T) -> String,
    onConfirm: (T) -> Unit,
    onDismiss: () -> Unit
) {
    var selected by remember { mutableStateOf(selectedItem) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = {
            SegmentedColumn(
                modifier = Modifier.verticalScroll(rememberScrollState()),
                content = items.map { item ->
                    {
                        SegmentedRadioItem(
                            title = itemTitle(item),
                            selected = selected == item,
                            onClick = { selected = item }
                        )
                    }
                }
            )
        },
        confirmButton = {
            TextButton(
                onClick = {
                    onConfirm(selected)
                    onDismiss()
                }
            ) {
                Text(stringResource(R.string.confirm))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(android.R.string.cancel))
            }
        }
    )
}
