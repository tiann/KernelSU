package me.weishu.kernelsu.ui.screen

import android.app.Activity.RESULT_OK
import android.content.Intent
import android.util.Log
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.google.accompanist.swiperefresh.SwipeRefresh
import com.google.accompanist.swiperefresh.rememberSwipeRefreshState
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.ConfirmDialog
import me.weishu.kernelsu.ui.component.DialogResult
import me.weishu.kernelsu.ui.component.rememberDialogHostState
import me.weishu.kernelsu.ui.screen.destinations.InstallScreenDestination
import me.weishu.kernelsu.ui.util.*
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Destination
@Composable
fun ModuleScreen(navigator: DestinationsNavigator) {
    val viewModel = viewModel<ModuleViewModel>()
    val snackBarHost = LocalSnackbarHost.current

    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        if (viewModel.moduleList.isEmpty()) {
            viewModel.fetchModuleList()
        }
    }

    val isSafeMode = Natives.isSafeMode()
    val hasMagisk = hasMagisk()

    val hideInstallButton = isSafeMode || hasMagisk

    Scaffold(topBar = {
        TopBar()
    }, floatingActionButton = if (hideInstallButton) {
        { /* Empty */ }
    } else {
        {
            val moduleInstall = stringResource(id = R.string.module_install)
            val selectZipLauncher = rememberLauncherForActivityResult(
                contract = ActivityResultContracts.StartActivityForResult()
            ) {
                if (it.resultCode != RESULT_OK) {
                    return@rememberLauncherForActivityResult
                }
                val data = it.data ?: return@rememberLauncherForActivityResult
                val uri = data.data ?: return@rememberLauncherForActivityResult

                navigator.navigate(InstallScreenDestination(uri))

                Log.i("ModuleScreen", "select zip result: ${it.data}")
            }

            ExtendedFloatingActionButton(
                onClick = {
                    // select the zip file to install
                    val intent = Intent(Intent.ACTION_GET_CONTENT)
                    intent.type = "application/zip"
                    selectZipLauncher.launch(intent)
                },
                icon = { Icon(Icons.Filled.Add, moduleInstall) },
                text = { Text(text = moduleInstall) },
            )
        }
    }) { innerPadding ->

        val dialogState = rememberDialogHostState()
        ConfirmDialog(dialogState)

        val failedEnable = stringResource(R.string.module_failed_to_enable)
        val failedDisable = stringResource(R.string.module_failed_to_disable)
        val failedUninstall = stringResource(R.string.module_uninstall_failed)
        val successUninstall = stringResource(R.string.module_uninstall_success)
        val swipeState = rememberSwipeRefreshState(viewModel.isRefreshing)
        // TODO: Replace SwipeRefresh with RefreshIndicator when it's ready
        if (Natives.getVersion() < 8) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(stringResource(R.string.require_kernel_version_8))
            }
            return@Scaffold
        }
        if (hasMagisk) {
            Box(modifier = Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
                Text(stringResource(R.string.module_magisk_conflict))
            }
            return@Scaffold
        }
        SwipeRefresh(
            state = swipeState, onRefresh = {
                scope.launch { viewModel.fetchModuleList() }
            }, modifier = Modifier
                .padding(innerPadding)
                .padding(16.dp)
                .fillMaxSize()
        ) {
            val isOverlayAvailable = overlayFsAvailable()
            if (!isOverlayAvailable) {
                swipeState.isRefreshing = false
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(stringResource(R.string.module_overlay_fs_not_available))
                }
                return@SwipeRefresh
            }
            val isEmpty = viewModel.moduleList.isEmpty()
            if (isEmpty) {
                swipeState.isRefreshing = false
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(stringResource(R.string.module_empty))
                }
            } else {
                LazyColumn(verticalArrangement = Arrangement.spacedBy(15.dp),
                    contentPadding = remember { PaddingValues(bottom = 16.dp + 56.dp /* Scaffold Fab Spacing + Fab container height */) }) {
                    items(viewModel.moduleList) { module ->
                        var isChecked by rememberSaveable(module) { mutableStateOf(module.enabled) }
                        val reboot = stringResource(id = R.string.reboot)
                        val rebootToApply = stringResource(id = R.string.reboot_to_apply)
                        val moduleStr = stringResource(id = R.string.module)
                        val uninstall = stringResource(id = R.string.uninstall)
                        val cancel = stringResource(id = android.R.string.cancel)
                        val moduleUninstallConfirm =
                            stringResource(id = R.string.module_uninstall_confirm)
                        ModuleItem(module, isChecked, onUninstall = {
                            scope.launch {
                                val dialogResult = dialogState.showDialog(
                                    moduleStr,
                                    content = moduleUninstallConfirm.format(module.name),
                                    confirm = uninstall,
                                    dismiss = cancel
                                )
                                if (dialogResult != DialogResult.Confirmed) {
                                    return@launch
                                }

                                val success = uninstallModule(module.id)
                                if (success) {
                                    viewModel.fetchModuleList()
                                }
                                val message = if (success) {
                                    successUninstall.format(module.name)
                                } else {
                                    failedUninstall.format(module.name)
                                }
                                val actionLabel = if (success) {
                                    reboot
                                } else {
                                    null
                                }
                                val result = snackBarHost.showSnackbar(
                                    message, actionLabel = actionLabel
                                )
                                if (result == SnackbarResult.ActionPerformed) {
                                    reboot()
                                }
                            }
                        }, onCheckChanged = {
                            val success = toggleModule(module.id, !isChecked)
                            if (success) {
                                isChecked = it
                                scope.launch {
                                    viewModel.fetchModuleList()

                                    val result = snackBarHost.showSnackbar(
                                        rebootToApply, actionLabel = reboot
                                    )
                                    if (result == SnackbarResult.ActionPerformed) {
                                        reboot()
                                    }
                                }
                            } else scope.launch {
                                val message = if (isChecked) failedDisable else failedEnable
                                snackBarHost.showSnackbar(message.format(module.name))
                            }
                        })
                        // fix last item shadow incomplete in LazyColumn
                        Spacer(Modifier.height(1.dp))
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar() {
    TopAppBar(title = { Text(stringResource(R.string.module)) })
}

@Composable
private fun ModuleItem(
    module: ModuleViewModel.ModuleInfo,
    isChecked: Boolean,
    onUninstall: (ModuleViewModel.ModuleInfo) -> Unit,
    onCheckChanged: (Boolean) -> Unit
) {
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {

        val textDecoration = if (!module.remove) null else TextDecoration.LineThrough

        Column(modifier = Modifier.padding(24.dp, 16.dp, 24.dp, 0.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                val moduleVersion = stringResource(id = R.string.module_version)
                val moduleAuthor = stringResource(id = R.string.module_author)

                Column(modifier = Modifier.fillMaxWidth(0.8f)) {
                    Text(
                        text = module.name,
                        fontSize = MaterialTheme.typography.titleMedium.fontSize,
                        fontWeight = FontWeight.SemiBold,
                        lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
                        fontFamily = MaterialTheme.typography.titleMedium.fontFamily,
                        textDecoration = textDecoration,
                    )

                    Text(
                        text = "$moduleVersion: ${module.version}",
                        fontSize = MaterialTheme.typography.bodySmall.fontSize,
                        lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
                        fontFamily = MaterialTheme.typography.bodySmall.fontFamily,
                        textDecoration = textDecoration
                    )

                    Text(
                        text = "$moduleAuthor: ${module.author}",
                        fontSize = MaterialTheme.typography.bodySmall.fontSize,
                        lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
                        fontFamily = MaterialTheme.typography.bodySmall.fontFamily,
                        textDecoration = textDecoration
                    )
                }

                Spacer(modifier = Modifier.weight(1f))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End,
                ) {
                    Switch(
                        enabled = !module.update,
                        checked = isChecked,
                        onCheckedChange = onCheckChanged
                    )
                }
            }

            Spacer(modifier = Modifier.height(12.dp))

            Text(
                text = module.description,
                fontSize = MaterialTheme.typography.bodySmall.fontSize,
                fontFamily = MaterialTheme.typography.bodySmall.fontFamily,
                lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
                fontWeight = MaterialTheme.typography.bodySmall.fontWeight,
                overflow = TextOverflow.Ellipsis,
                maxLines = 4,
                textDecoration = textDecoration
            )


            Spacer(modifier = Modifier.height(16.dp))

            Divider(thickness = Dp.Hairline)

            Row(
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Spacer(modifier = Modifier.weight(1f, true))

                TextButton(
                    enabled = !module.remove,
                    onClick = { onUninstall(module) },
                ) {
                    Text(
                        fontFamily = MaterialTheme.typography.labelMedium.fontFamily,
                        fontSize = MaterialTheme.typography.labelMedium.fontSize,
                        text = stringResource(R.string.uninstall),
                    )
                }
            }
        }
    }
}

@Preview
@Composable
fun ModuleItemPreview() {
    val module = ModuleViewModel.ModuleInfo(
        id = "id",
        name = "name",
        version = "version",
        versionCode = 1,
        author = "author",
        description = "I am a test module and i do nothing but show a very long description",
        enabled = true,
        update = true,
        remove = true,
    )
    ModuleItem(module, true, {}, {})
}