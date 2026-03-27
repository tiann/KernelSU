package me.weishu.kernelsu.ui.screen.templateeditor

import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.compose.dropUnlessResumed
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.util.deleteAppProfileTemplate
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel

@Composable
fun TemplateEditorScreen(template: TemplateViewModel.TemplateInfo, readOnly: Boolean) {
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val uiMode = LocalUiMode.current
    val isCreation = template.id.isBlank()
    val autoSave = uiMode == UiMode.Miuix && !isCreation

    var currentTemplate by rememberSaveable { mutableStateOf(template) }
    var idErrorHint by remember { mutableStateOf("") }

    val saveTemplateFailed = stringResource(id = R.string.app_profile_template_save_failed)
    val idConflictError = stringResource(id = R.string.app_profile_template_id_exist)
    val idInvalidError = stringResource(id = R.string.app_profile_template_id_invalid)

    fun showToast(message: String) {
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
    }

    fun finishEditing() {
        if (!readOnly) {
            navigator.setResult("template_edit", true)
        } else {
            navigator.pop()
        }
    }

    fun updateTemplate(updatedTemplate: TemplateViewModel.TemplateInfo) {
        if (autoSave) {
            if (!saveTemplate(updatedTemplate)) {
                return
            }
        }
        currentTemplate = updatedTemplate
    }

    val uiState = TemplateEditorUiState(
        template = currentTemplate,
        initialTemplate = template,
        readOnly = readOnly,
        isCreation = isCreation,
        idErrorHint = idErrorHint,
    )

    fun saveCurrentTemplate() {
        if (uiMode == UiMode.Miuix) {
            when (idCheck(currentTemplate.id)) {
                1 -> {
                    showToast(idConflictError)
                    return
                }

                2 -> {
                    showToast(idInvalidError)
                    return
                }
            }
        }

        if (saveTemplate(currentTemplate, isCreation)) {
            navigator.setResult("template_edit", true)
        } else {
            showToast(saveTemplateFailed)
        }
    }

    val actions = TemplateEditorActions(
        onBack = dropUnlessResumed { finishEditing() },
        onDelete = {
            if (deleteAppProfileTemplate(currentTemplate.id)) {
                navigator.setResult("template_edit", true)
            }
        },
        onSave = ::saveCurrentTemplate,
        onNameChange = { value ->
            updateTemplate(currentTemplate.copy(name = value))
        },
        onIdChange = { value ->
            idErrorHint = if (isTemplateExist(value)) {
                idConflictError
            } else if (!isValidTemplateId(value)) {
                idInvalidError
            } else {
                ""
            }
            currentTemplate = currentTemplate.copy(id = value)
        },
        onAuthorChange = { value ->
            updateTemplate(currentTemplate.copy(author = value))
        },
        onDescriptionChange = { value ->
            updateTemplate(currentTemplate.copy(description = value))
        },
        onProfileChange = { profile ->
            updateTemplate(
                currentTemplate.copy(
                    uid = profile.uid,
                    gid = profile.gid,
                    groups = profile.groups,
                    capabilities = profile.capabilities,
                    context = profile.context,
                    namespace = profile.namespace,
                    rules = profile.rules.split("\n"),
                )
            )
        },
    )

    when (uiMode) {
        UiMode.Miuix -> TemplateEditorScreenMiuix(
            state = uiState,
            actions = actions,
        )

        UiMode.Material -> TemplateEditorScreenMaterial(
            state = uiState,
            actions = actions,
        )
    }
}
