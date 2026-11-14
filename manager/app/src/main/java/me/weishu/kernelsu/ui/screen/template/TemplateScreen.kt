package me.weishu.kernelsu.ui.screen.template

import android.content.ClipData
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.util.isNetworkAvailable
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel
import top.yukonga.miuix.kmp.basic.SnackbarHostState as MiuixSnackbarHostState

@Composable
fun AppProfileTemplateScreen() {
    val navigator = LocalNavigator.current
    val viewModel = viewModel<TemplateViewModel>()
    val screenState by viewModel.uiState.collectAsStateWithLifecycle()
    val clipboard = LocalClipboard.current
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val requestKey = "template_edit"
    val snackBarHost = remember { SnackbarHostState() }
    val miuixSnackbarHost = remember { MiuixSnackbarHostState() }

    LaunchedEffect(Unit) {
        if (screenState.templateList.isEmpty()) {
            viewModel.fetchTemplates()
        }
    }

    LaunchedEffect(Unit) {
        navigator.observeResult<Boolean>(requestKey).collect { success ->
            if (success) {
                viewModel.fetchTemplates()
            }
        }
    }

    val importEmptyText = stringResource(R.string.app_profile_template_import_empty)
    val importSuccessText = stringResource(R.string.app_profile_template_import_success)
    val exportEmptyText = stringResource(R.string.app_profile_template_export_empty)

    fun showMessage(message: String) {
        scope.launch {
            snackBarHost.showSnackbar(message)

        }
    }

    val uiState = screenState.copy(offline = !isNetworkAvailable(context))
    val actions = TemplateActions(
        onBack = dropUnlessResumed { navigator.pop() },
        onRefresh = { forceSync ->
            scope.launch {
                viewModel.fetchTemplates(forceSync)
            }
        },
        onImport = {
            scope.launch {
                clipboard.getClipEntry()?.clipData?.getItemAt(0)?.text?.toString()?.let { templateText ->
                    if (templateText.isEmpty()) {
                        showMessage(importEmptyText)
                        return@let
                    }
                    viewModel.importTemplates(
                        templateText,
                        onSuccess = {
                            showMessage(importSuccessText)
                            viewModel.fetchTemplates(false)
                        },
                        onFailure = { showMessage(it) },
                    )
                }
            }
        },
        onExport = {
            scope.launch {
                viewModel.exportTemplates(
                    onTemplateEmpty = {
                        showMessage(exportEmptyText)
                    },
                    callback = { templateText ->
                        clipboard.setClipEntry(
                            ClipEntry(ClipData.newPlainText("template", templateText))
                        )
                    },
                )
            }
        },
        onCreateTemplate = {
            navigator.push(
                Route.TemplateEditor(TemplateViewModel.TemplateInfo(), false)
            )
        },
        onOpenTemplate = { template ->
            navigator.push(
                Route.TemplateEditor(template, !template.local)
            )
        },
    )

    AppProfileTemplateScreenMaterial(
        state = uiState,
        actions = actions,
        snackBarHost = snackBarHost,
    )
}
