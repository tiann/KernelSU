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
        SwipeRefresh(
            state = swipeState,
            onRefresh = {
                scope.launch { viewModel.fetchModuleList() }
            },
            modifier = Modifier
                .padding(innerPadding)
                .padding(16.dp)
                .fillMaxSize()
        ) {
            val isEmpty = viewModel.moduleList.isEmpty()
            if (isEmpty) {
                swipeState.isRefreshing = false
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(stringResource(R.string.module_empty))
                }
            } else {
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(15.dp)
                ) {
                    items(viewModel.moduleList) { module ->
                        var isChecked by rememberSaveable(module) { mutableStateOf(module.enabled) }
                        ModuleItem(module,
                            isChecked,
                            onUninstall = {
                                scope.launch {
                                    val result = uninstallModule(module.id)
                                    if (result) {
                                        viewModel.fetchModuleList()
                                    }
                                    snackBarHost.showSnackbar(
                                        if (result) {
                                            successUninstall.format(module.name)
                                        } else {
                                            failedUninstall.format(module.name)
                                        }
                                    )
                                }
                            },
                            onCheckChanged = {
                                val success = toggleModule(module.id, !isChecked)
                                if (success) {
                                    isChecked = it
                                    scope.launch {
                                        viewModel.fetchModuleList()
                                    }
                                } else scope.launch {
                                    val message = if (isChecked) failedDisable else failedEnable
                                    snackBarHost.showSnackbar(message.format(module.name))
                                }
                            }
                        )
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