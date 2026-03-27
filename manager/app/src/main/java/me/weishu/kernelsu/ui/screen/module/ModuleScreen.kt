package me.weishu.kernelsu.ui.screen.module

import android.Manifest
import android.content.Intent
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.unit.Dp
import androidx.core.net.toUri
import androidx.lifecycle.compose.LifecycleResumeEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import me.weishu.kernelsu.ui.webui.WebUIActivity

@Composable
fun ModulePager(
    bottomInnerPadding: Dp,
    isCurrentPage: Boolean = true
) {
    val uiMode = LocalUiMode.current
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val resource = LocalResources.current
    val viewModel = viewModel<ModuleViewModel>()
    val rawUiState by viewModel.uiState.collectAsStateWithLifecycle()

    val webUILauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { viewModel.fetchModuleList() }

    // Request notification permission for download progress notifications
    val notificationPermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission()
    ) { /* Download works regardless of result */ }

    var hasActivated by remember { mutableStateOf(false) }
    if (isCurrentPage) hasActivated = true

    if (hasActivated) {
        LaunchedEffect(Unit) {
            viewModel.refreshEnvironmentState()
            viewModel.initializePreferences()
            if (Build.VERSION.SDK_INT >= 33) {
                notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }

        LifecycleResumeEffect(Unit) {
            viewModel.fetchModuleList(checkUpdate = rawUiState.moduleList.isEmpty() || viewModel.isNeedRefresh)
            onPauseOrDispose {}
        }
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
            viewModel.requestUpdateConfirmation(module, updateInfo)
        },
        onRequestUninstallConfirmation = { module ->
            viewModel.requestUninstallConfirmation(module)
        },
        onDismissConfirmRequest = {
            viewModel.dismissConfirmRequest()
        },
        onConsumeEffect = {
            viewModel.consumeEffect()
        },
        onConfirmUpdate = { request ->
            download(
                url = request.downloadUrl,
                fileName = request.fileName,
                onDownloaded = { uri ->
                    navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri))))
                    viewModel.markNeedRefresh()
                },
                onDownloading = {
                    viewModel.emitEffect(
                        ModuleEffect.Toast(
                            resource.getString(R.string.module_downloading).format(request.module.name)
                        )
                    )
                },
            )
            viewModel.dismissConfirmRequest()
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
            viewModel.toggleModule(module)
        },
        onUninstallModule = { module ->
            viewModel.uninstallModule(module)
        },
        onUndoUninstallModule = { module ->
            viewModel.undoUninstallModule(module)
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
            confirmDialogState = rawUiState.confirmDialogState,
            effect = rawUiState.effect,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )

        UiMode.Material -> ModulePagerMaterial(
            uiState = rawUiState,
            confirmDialogState = rawUiState.confirmDialogState,
            effect = rawUiState.effect,
            actions = actions,
            bottomInnerPadding = bottomInnerPadding,
        )
    }
}
