package me.weishu.kernelsu.ui.screen.module

import android.widget.Toast
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.items
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.ButtonDefaults
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.data.model.ModuleUpdateInfo
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearPrimaryButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun ModulePagerWear(
    uiState: ModuleUiState,
    confirmDialogState: ModuleConfirmDialogState?,
    effect: ModuleEffect?,
    actions: ModuleActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val context = LocalContext.current
    val horizontalPadding = wearHorizontalPadding()
    var expandedModuleId by remember { mutableStateOf<String?>(null) }
    val modules =
        if (uiState.searchStatus.searchText.isNotEmpty()) uiState.searchResults else uiState.moduleList

    LaunchedEffect(effect) {
        when (effect) {
            is ModuleEffect.Toast -> {
                Toast.makeText(context, effect.message, Toast.LENGTH_SHORT).show()
                actions.onConsumeEffect()
            }

            is ModuleEffect.SnackBar -> {
                Toast.makeText(context, effect.message, Toast.LENGTH_SHORT).show()
                actions.onConsumeEffect()
            }

            null -> Unit
        }
    }

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.module))
                }
            }

            if (uiState.magiskInstalled) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.module_magisk_conflict),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        isError = true,
                    )
                }
            } else {
                if (uiState.installButtonVisible) {
                    item {
                        WearPrimaryButton(
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            onClick = actions.onOpenRepo,
                            label = stringResource(R.string.module_repos),
                        )
                    }
                } else if (uiState.isSafeMode) {
                    item {
                        WearMessageText(
                            text = stringResource(R.string.safe_mode_module_disabled),
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            isError = true,
                        )
                    }
                }

                if (confirmDialogState != null) {
                    item {
                        ModuleWearConfirmSection(
                            confirmDialogState = confirmDialogState,
                            horizontalPadding = horizontalPadding,
                            onConfirm = {
                                when (val request = confirmDialogState.request) {
                                    is ModuleConfirmRequest.Uninstall -> actions.onUninstallModule(
                                        request.module
                                    )

                                    is ModuleConfirmRequest.Update -> actions.onConfirmUpdate(
                                        request
                                    )
                                }
                            },
                            onDismiss = actions.onDismissConfirmRequest,
                        )
                    }
                }

                if (uiState.isRefreshing || !uiState.hasLoaded) {
                    item {
                        WearMessageText(
                            text = if (uiState.isRefreshing) stringResource(R.string.processing) else "",
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        )
                    }
                } else if (modules.isEmpty()) {
                    item {
                        WearMessageText(
                            text = stringResource(R.string.module_empty),
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        )
                    }
                } else {
                    items(modules, key = { it.id }) { module ->
                        ModuleWearItem(
                            module = module,
                            updateInfo = uiState.updateInfo[module.id],
                            expanded = expandedModuleId == module.id,
                            horizontalPadding = horizontalPadding,
                            onToggleExpand = {
                                expandedModuleId =
                                    if (expandedModuleId == module.id) null else module.id
                            },
                            actions = actions,
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ModuleWearConfirmSection(
    confirmDialogState: ModuleConfirmDialogState,
    horizontalPadding: androidx.compose.ui.unit.Dp,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text = confirmDialogState.title,
            style = MaterialTheme.typography.titleSmall,
            modifier = Modifier.padding(horizontal = horizontalPadding),
        )
        confirmDialogState.content?.let {
            Text(
                text = it,
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.padding(horizontal = horizontalPadding),
            )
        }
        Button(
            modifier = Modifier
                .padding(horizontal = horizontalPadding)
                .fillMaxWidth(),
            onClick = onConfirm,
            label = { Text(confirmDialogState.confirm ?: stringResource(R.string.confirm)) },
        )
        Button(
            modifier = Modifier
                .padding(horizontal = horizontalPadding)
                .fillMaxWidth(),
            onClick = onDismiss,
            colors = ButtonDefaults.filledTonalButtonColors(),
            label = { Text(confirmDialogState.dismiss ?: stringResource(R.string.undo)) },
        )
    }
}

@Composable
private fun ModuleWearItem(
    module: Module,
    updateInfo: ModuleUpdateInfo?,
    expanded: Boolean,
    horizontalPadding: androidx.compose.ui.unit.Dp,
    onToggleExpand: () -> Unit,
    actions: ModuleActions,
) {
    val secondary = buildString {
        append(module.version)
        append(" • ")
        append(module.author)
        if (module.update) append(" • ${stringResource(R.string.module_update)}")
        if (module.remove) append(" • ${stringResource(R.string.uninstall)}")
        if (module.metamodule) append(" • Meta")
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Button(
            modifier = Modifier
                .padding(horizontal = horizontalPadding)
                .fillMaxWidth(),
            onClick = onToggleExpand,
            label = { Text(module.name, maxLines = 1) },
            secondaryLabel = { Text(secondary, maxLines = 2) },
            colors = if (module.enabled) {
                ButtonDefaults.buttonColors()
            } else {
                ButtonDefaults.filledTonalButtonColors()
            },
        )

        if (expanded) {
            if (module.update && updateInfo != null) {
                Button(
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                    onClick = { actions.onRequestUpdateConfirmation(module, updateInfo) },
                    label = { Text(stringResource(R.string.module_update)) },
                )
            }

            Button(
                modifier = Modifier
                    .padding(horizontal = horizontalPadding)
                    .fillMaxWidth(),
                onClick = { actions.onToggleModule(module) },
                colors = ButtonDefaults.filledTonalButtonColors(),
                label = {
                    Text(
                        if (module.enabled) stringResource(R.string.disable) else stringResource(
                            R.string.enable
                        )
                    )
                },
            )

            if (module.hasWebUi) {
                Button(
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                    onClick = { actions.onOpenWebUi(module) },
                    colors = ButtonDefaults.filledTonalButtonColors(),
                    label = { Text(stringResource(R.string.webui)) },
                )
            }

            if (module.hasActionScript) {
                Button(
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                    onClick = { actions.onExecuteModuleAction(module) },
                    colors = ButtonDefaults.filledTonalButtonColors(),
                    label = { Text(stringResource(R.string.module_action)) },
                )
            }

            Button(
                modifier = Modifier
                    .padding(horizontal = horizontalPadding)
                    .fillMaxWidth(),
                onClick = {
                    if (module.remove) {
                        actions.onUndoUninstallModule(module)
                    } else {
                        actions.onRequestUninstallConfirmation(module)
                    }
                },
                colors = ButtonDefaults.filledTonalButtonColors(),
                label = {
                    Text(
                        if (module.remove) stringResource(R.string.undo) else stringResource(
                            R.string.uninstall
                        )
                    )
                },
            )
        }
    }
}
