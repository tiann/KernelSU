package me.weishu.kernelsu.ui.screen.module

import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.unit.Dp
import androidx.core.net.toUri
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.data.model.ModuleUpdateInfo
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.module.fetchModuleDetail
import me.weishu.kernelsu.ui.util.module.fetchReleaseDescriptionHtml
import me.weishu.kernelsu.ui.util.toggleModule
import me.weishu.kernelsu.ui.util.undoUninstallModule
import me.weishu.kernelsu.ui.util.uninstallModule
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import me.weishu.kernelsu.ui.webui.WebUIActivity
import okhttp3.Request

@Composable
fun ModulePager(
    bottomInnerPadding: Dp
) {
    val uiMode = LocalUiMode.current
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val resource = LocalResources.current
    val viewModel = viewModel<ModuleViewModel>()
    val rawUiState by viewModel.uiState.collectAsState()
    val scope = rememberCoroutineScope()
    var confirmDialogState by remember { mutableStateOf<ModuleConfirmDialogState?>(null) }
    var effect by remember { mutableStateOf<ModuleEffect?>(null) }

    val webUILauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { viewModel.fetchModuleList() }

    LaunchedEffect(Unit) {
        viewModel.refreshEnvironmentState()
        viewModel.initializePreferences()

        if (rawUiState.moduleList.isEmpty() || viewModel.isNeedRefresh) {
            viewModel.fetchModuleList(checkUpdate = true)
        }
    }

    suspend fun buildUpdateConfirmDialogState(
        module: Module,
        updateInfo: ModuleUpdateInfo,
    ): ModuleConfirmDialogState {
        val changelogUrl = updateInfo.changelog

        var changelog = ""
        var html = false

        if (changelogUrl.isNotBlank()) {
            withContext(Dispatchers.IO) {
                if (changelogUrl.startsWith("#") && changelogUrl.contains('@')) {
                    val parts = changelogUrl.substring(1).split('@', limit = 2)
                    if (parts.size == 2) {
                        fetchReleaseDescriptionHtml(parts[0], parts[1])?.let {
                            changelog = it
                            html = true
                        }
                    }
                }

                if (changelog.isBlank()) {
                    changelog = runCatching {
                        ksuApp.okhttpClient.newCall(
                            Request.Builder().url(changelogUrl).build()
                        ).execute().body.string()
                    }.getOrDefault("")
                }
            }
        }

        if (changelog.isBlank()) {
            withContext(Dispatchers.IO) {
                runCatching {
                    val latestTag = fetchModuleDetail(module.id)?.latestTag.orEmpty()
                    if (latestTag.isNotBlank()) {
                        fetchReleaseDescriptionHtml(module.id, latestTag)?.let {
                            changelog = it
                            html = true
                        }
                    }
                }
            }
        }

        return ModuleConfirmDialogState(
            request = ModuleConfirmRequest.Update(
                module = module,
                downloadUrl = updateInfo.downloadUrl,
                fileName = "${module.name}-${updateInfo.version}.zip",
            ),
            title = if (changelog.isNotBlank()) resource.getString(R.string.module_changelog) else resource.getString(R.string.module_update),
            content = changelog.ifBlank { resource.getString(R.string.module_start_downloading).format(module.name) },
            markdown = changelog.isNotBlank() && !html,
            html = html,
            confirm = resource.getString(R.string.module_update),
        )
    }

    val actions = ModuleActions(
        onRefresh = {
            viewModel.fetchModuleList(checkUpdate = true)
        },
        onSearchStatusChange = {
            viewModel.updateSearchStatus(it)
        },
        onSearchTextChange = { text ->
            viewModel.updateSearchText(text)
        },
        onClearSearch = {
            viewModel.updateSearchText("")
        },
        onRequestUpdateConfirmation = { module, updateInfo ->
            scope.launch {
                confirmDialogState = buildUpdateConfirmDialogState(module, updateInfo)
            }
        },
        onRequestUninstallConfirmation = { module ->
            confirmDialogState = ModuleConfirmDialogState(
                request = ModuleConfirmRequest.Uninstall(module),
                title = resource.getString(R.string.module),
                content = (if (module.metamodule) resource.getString(R.string.metamodule_uninstall_confirm) else resource.getString(R.string.module_uninstall_confirm)).format(
                    module.name
                ),
                confirm = resource.getString(R.string.uninstall),
                dismiss = resource.getString(android.R.string.cancel),
            )
        },
        onDismissConfirmRequest = {
            confirmDialogState = null
        },
        onConsumeEffect = {
            effect = null
        },
        onConfirmUpdate = { request ->
            scope.launch {
                withContext(Dispatchers.IO) {
                    download(
                        url = request.downloadUrl,
                        fileName = request.fileName,
                        onDownloaded = { uri ->
                            navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri))))
                            viewModel.markNeedRefresh()
                        },
                        onDownloading = {
                            effect = ModuleEffect.Toast(resource.getString(R.string.module_downloading).format(request.module.name))
                        },
                    )
                }
                confirmDialogState = null
            }
        },
        onOpenRepo = { navigator.push(Route.ModuleRepo) },
        onToggleSortActionFirst = {
            viewModel.toggleSortActionFirst()
        },
        onToggleSortEnabledFirst = {
            viewModel.toggleSortEnabledFirst()
        },
        onOpenWebUi = { module ->
            webUILauncher.launch(
                Intent(context, WebUIActivity::class.java)
                    .setData("kernelsu://webui/${module.id}".toUri())
                    .putExtra("id", module.id)
            )
        },
        onToggleModule = { module ->
            scope.launch {
                val success = withContext(Dispatchers.IO) {
                    toggleModule(module.id, !module.enabled)
                }
                if (success) {
                    viewModel.fetchModuleList(checkUpdate = true)
                    effect = ModuleEffect.SnackBar(resource.getString(R.string.reboot_to_apply))
                } else {
                    val message = if (module.enabled) R.string.module_failed_to_disable else R.string.module_failed_to_enable
                    effect = ModuleEffect.SnackBar(resource.getString(message).format(module.name))
                }
            }
        },
        onUninstallModule = { module ->
            scope.launch {
                val success = withContext(Dispatchers.IO) {
                    uninstallModule(module.id)
                }
                if (success) {
                    viewModel.fetchModuleList(checkUpdate = true)
                }
                confirmDialogState = null
                effect = ModuleEffect.SnackBar(
                    resource.getString(
                        if (success) R.string.module_uninstall_success else R.string.module_uninstall_failed
                    ).format(module.name)
                )
            }
        },
        onUndoUninstallModule = { module ->
            scope.launch {
                val success = withContext(Dispatchers.IO) {
                    undoUninstallModule(module.id)
                }
                if (success) {
                    viewModel.fetchModuleList(checkUpdate = true)
                }
                effect = ModuleEffect.SnackBar(
                    resource.getString(
                        if (success) R.string.module_undo_uninstall_success else R.string.module_undo_uninstall_failed
                    ).format(module.name)
                )
            }
        },
        onOpenFlash = { uris ->
            if (uris.isNotEmpty()) {
                navigator.push(Route.Flash(FlashIt.FlashModules(uris)))
                viewModel.markNeedRefresh()
            }
        },
        onExecuteModuleAction = { module ->
            navigator.push(Route.ExecuteModuleAction(module.id))
            viewModel.markNeedRefresh()
        },
    )

    when (uiMode) {
        UiMode.Miuix -> ModulePagerMiuix(
            uiState = rawUiState,
            confirmDialogState = confirmDialogState,
            effect = effect,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )

        UiMode.Material -> ModulePagerMaterial(
            uiState = rawUiState,
            confirmDialogState = confirmDialogState,
            effect = effect,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )
    }
}
