package me.weishu.kernelsu.ui.component.profile.dialogs

import android.annotation.SuppressLint
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
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
import me.weishu.kernelsu.ui.component.material.ExpressiveCheckboxItem
import me.weishu.kernelsu.ui.component.material.ExpressiveColumn

@SuppressLint("MutableCollectionMutableState")
@Composable
fun <T> MultiSelectDialog(
    title: String,
    subtitle: String? = null,
    items: List<T>,
    selectedItems: Set<T>,
    itemTitle: (T) -> String,
    itemSubtitle: (T) -> String?,
    maxSelection: Int = Int.MAX_VALUE,
    onConfirm: (Set<T>) -> Unit,
    onDismiss: () -> Unit
) {
    var currentSelection by remember { mutableStateOf(selectedItems.toMutableSet()) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Column {
                Text(title)
                if (subtitle != null) {
                    Text(
                        text = subtitle,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        },
        text = {
            ExpressiveColumn(
                modifier = Modifier.verticalScroll(rememberScrollState()),
                content = items.map { item ->
                    {
                        ExpressiveCheckboxItem(
                            title = itemTitle(item),
                            summary = itemSubtitle(item),
                            checked = currentSelection.contains(item),
                            onCheckedChange = { isChecked ->
                                val newSelection = currentSelection.toMutableSet()
                                if (isChecked && newSelection.size < maxSelection) {
                                    newSelection.add(item)
                                } else if (!isChecked) {
                                    newSelection.remove(item)
                                }
                                currentSelection = newSelection
                            }
                        )
                    }
                }
            )
        },
        confirmButton = {
            TextButton(
                onClick = { onConfirm(currentSelection.toSet()) }
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
