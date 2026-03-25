package me.weishu.kernelsu.ui.screen.template

import android.content.ClipData
import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.util.isNetworkAvailable
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel

@Composable
fun AppProfileTemplateScreen() {
    val uiMode = LocalUiMode.current
    val navigator = LocalNavigator.current
    val viewModel = viewModel<TemplateViewModel>()
    val screenState by viewModel.uiState.collectAsStateWithLifecycle()
    val clipboard = LocalClipboard.current
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val requestKey = "template_edit"

    LaunchedEffect(Unit) {
        if (screenState.templateList.isEmpty()) {
            viewModel.fetchTemplates()
        }
    }

    LaunchedEffect(Unit) {
        navigator.observeResult<Boolean>(requestKey).collect { success ->
            if (success) {
                if (uiMode == UiMode.Miuix) {
                    navigator.clearResult(requestKey)
                }
                viewModel.fetchTemplates()
            }
        }
    }

    val importEmptyText = stringResource(R.string.app_profile_template_import_empty)
    val importSuccessText = stringResource(R.string.app_profile_template_import_success)
    val exportEmptyText = stringResource(R.string.app_profile_template_export_empty)

    val showToast: (String) -> Unit = { message ->
        scope.launch(Dispatchers.Main) {
            Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
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
                        showToast(importEmptyText)
                        return@let
                    }
                    viewModel.importTemplates(
                        templateText,
                        onSuccess = {
                            showToast(importSuccessText)
                            viewModel.fetchTemplates(false)
                        },
                        onFailure = showToast,
                    )
                }
            }
        },
        onExport = {
            scope.launch {
                viewModel.exportTemplates(
                    onTemplateEmpty = {
                        showToast(exportEmptyText)
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
            when (uiMode) {
                UiMode.Miuix -> navigator.navigateForResult(
                    Route.TemplateEditor(TemplateViewModel.TemplateInfo(), false),
                    requestKey,
                )

                UiMode.Material -> navigator.push(
                    Route.TemplateEditor(TemplateViewModel.TemplateInfo(), false)
                )
            }
        },
        onOpenTemplate = { template ->
            when (uiMode) {
                UiMode.Miuix -> navigator.navigateForResult(
                    Route.TemplateEditor(template, !template.local),
                    requestKey,
                )

                UiMode.Material -> navigator.push(
                    Route.TemplateEditor(template, !template.local)
                )
            }
        },
    )

    when (uiMode) {
        UiMode.Miuix -> AppProfileTemplateScreenMiuix(
            state = uiState,
            actions = actions,
        )

        UiMode.Material -> AppProfileTemplateScreenMaterial(
            state = uiState,
            actions = actions,
        )
    }
}
