package me.weishu.kernelsu.ui.component.uninstalldialog

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.material.ExpressiveColumn
import me.weishu.kernelsu.ui.component.material.ExpressiveListItem
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.screen.flash.UninstallType
import me.weishu.kernelsu.ui.screen.flash.UninstallType.PERMANENT
import me.weishu.kernelsu.ui.screen.flash.UninstallType.RESTORE_STOCK_IMAGE

@Composable
fun UninstallDialogMaterial(
    showDialog: MutableState<Boolean>,
    navigator: Navigator
) {
    val options = listOf(
        // TEMPORARY,
        PERMANENT,
        RESTORE_STOCK_IMAGE
    )
    val showConfirmDialog = remember { mutableStateOf(false) }
    val runType = remember { mutableStateOf<UninstallType?>(null) }

    val run = { type: UninstallType ->
        when (type) {
            PERMANENT -> navigator.push(Route.Flash(FlashIt.FlashUninstall))
            RESTORE_STOCK_IMAGE -> navigator.push(Route.Flash(FlashIt.FlashRestore))
            else -> Unit
        }
    }

    if (showDialog.value) {
        AlertDialog(
            onDismissRequest = { showDialog.value = false },
            title = { Text(stringResource(R.string.settings_uninstall)) },
            text = {
                ExpressiveColumn(
                    modifier = Modifier,
                    content = options.map { type ->
                        {
                           ExpressiveListItem(
                                onClick = {
                                    showConfirmDialog.value = true
                                    runType.value = type
                                },
                                headlineContent = { Text(stringResource(type.title)) },
                                supportingContent = { Text(stringResource(type.message)) },
                                leadingContent = {
                                    Icon(
                                        imageVector = type.icon,
                                        contentDescription = null
                                    )
                                }
                            )
                        }
                    }
                )
            },
            confirmButton = {
                TextButton(onClick = { showDialog.value = false }) {
                    Text(stringResource(android.R.string.cancel))
                }
            }
        )
    }

    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            showConfirmDialog.value = false
            showDialog.value = false
            runType.value?.let { type ->
                run(type)
            }
        },
        onDismiss = {
            showConfirmDialog.value = false
        }
    )

    val dialogTitle = runType.value?.let { type ->
        options.find { it == type }?.let { stringResource(it.title) }
    } ?: ""
    val dialogContent = runType.value?.let { type ->
        options.find { it == type }?.let { stringResource(it.message) }
    }

    if (showConfirmDialog.value) {
        confirmDialog.showConfirm(title = dialogTitle, content = dialogContent)
    }
}
