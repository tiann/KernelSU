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
import me.weishu.kernelsu.ui.screen.destinations.InstallScreenDestination
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.util.toggleModule
import me.weishu.kernelsu.ui.util.uninstallModule
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

    Scaffold(
        topBar = {
            TopBar()
        },
        floatingActionButton = {
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
        },
    ) { innerPadding ->
        val failedEnable = stringResource(R.string.module_failed_to_enable)
        val failedDisable = stringResource(R.string.module_failed_to_disable)
        val swipeState = rememberSwipeRefreshState(viewModel.isRefreshing)
        // TODO: Replace SwipeRefresh with RefreshIndicator when it's ready
        if (Natives.getVersion() < 8) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(stringResource(R.string.require_kernel_version_8))
            }
            return@Scaffold
        }
        SwipeRefresh(
            state = swipeState,
            onRefresh = {
                scope.launch { viewModel.fetchModuleList() }
            },
            modifier = Modifier
                .padding(innerPadding)
                .fillMaxSize()
        ) {
            val isEmpty = viewModel.moduleList.isEmpty()
            if (isEmpty) {
                swipeState.isRefreshing = false
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(stringResource(R.string.module_empty))
                }
            } else {
                LazyColumn {
                    items(viewModel.moduleList) { module ->
                        var isChecked by rememberSaveable(module) { mutableStateOf(module.enabled) }
                        ModuleItem(module,
                            isChecked,
                            onUninstall = {
                                scope.launch {
                                    val result = uninstallModule(module.id)
                                }
                            },
                            onCheckChanged = {
                                val success = toggleModule(module.id, !isChecked)
                                if (success) {
                                    isChecked = it
                                } else scope.launch {
                                    val message = if (isChecked) failedDisable else failedEnable
                                    snackBarHost.showSnackbar(message.format(module.name))
                                }
                            })
                    }
                }
            }

        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar() {
    TopAppBar(
        title = { Text(stringResource(R.string.module)) }
    )
}

@Composable
private fun ModuleItem(
    module: ModuleViewModel.ModuleInfo,
    isChecked: Boolean,
    onUninstall: (ModuleViewModel.ModuleInfo) -> Unit,
    onCheckChanged: (Boolean) -> Unit
) {
    ElevatedCard(
        modifier = Modifier
            .fillMaxWidth()
            .padding(8.dp),
        colors =
        CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {

        val textDecoration = if (!module.remove) null else TextDecoration.LineThrough

        Column(modifier = Modifier.padding(16.dp, 16.dp, 16.dp, 0.dp)) {
            Row {
                Column {
                    Text(
                        text = module.name,
                        fontSize = MaterialTheme.typography.titleLarge.fontSize,
                        fontFamily = MaterialTheme.typography.titleLarge.fontFamily,
                        textDecoration = textDecoration,
                    )

                    Row {
                        Text(
                            text = module.version,
                            fontFamily = MaterialTheme.typography.titleMedium.fontFamily,
                            fontSize = MaterialTheme.typography.titleMedium.fontSize,
                            textDecoration = textDecoration
                        )

                        Spacer(modifier = Modifier.width(8.dp))

                        Text(
                            text = module.author,
                            fontFamily = MaterialTheme.typography.titleMedium.fontFamily,
                            fontSize = MaterialTheme.typography.titleMedium.fontSize,
                            textDecoration = textDecoration
                        )

                    }
                }

                Spacer(modifier = Modifier.weight(1f))

                Switch(
                    enabled = !module.update,
                    checked = isChecked,
                    onCheckedChange = onCheckChanged
                )
            }

            Spacer(modifier = Modifier.height(12.dp))

            Text(
                text = module.description,
                fontFamily = MaterialTheme.typography.bodyMedium.fontFamily,
                fontSize = MaterialTheme.typography.bodyMedium.fontSize,
                lineHeight = MaterialTheme.typography.bodyMedium.lineHeight,
                fontWeight = MaterialTheme.typography.bodyMedium.fontWeight,
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
                    enabled = !module.update && !module.remove,
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