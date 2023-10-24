package me.weishu.kernelsu.ui.screen

import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.DeleteForever
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInteropFilter
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.result.ResultBackNavigator
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.util.deleteAppProfileTemplate
import me.weishu.kernelsu.ui.util.getAppProfileTemplate
import me.weishu.kernelsu.ui.util.setAppProfileTemplate
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel
import me.weishu.kernelsu.ui.viewmodel.toJSON
import org.json.JSONArray
import org.json.JSONObject

/**
 * @author weishu
 * @date 2023/10/20.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Destination
@Composable
fun TemplateEditorScreen(
    navigator: ResultBackNavigator<Boolean>,
    initialTemplate: TemplateViewModel.TemplateInfo,
    readOnly: Boolean = true,
) {

    val isCreation = initialTemplate.id.isBlank()
    val autoSave = !isCreation

    var template by rememberSaveable {
        mutableStateOf(initialTemplate)
    }

    BackHandler {
        navigator.navigateBack(result = !readOnly)
    }

    Scaffold(
        topBar = {
            val author =
                if (initialTemplate.author.isNotEmpty()) "@${initialTemplate.author}" else ""
            val readOnlyHint = if (readOnly) {
                " - ${stringResource(id = R.string.app_profile_template_readonly)}"
            } else {
                ""
            }
            val titleSummary = "${initialTemplate.id}$author$readOnlyHint"
            val saveTemplateFailed = stringResource(id = R.string.app_profile_template_save_failed)
            val context = LocalContext.current

            TopBar(
                title = if (isCreation) {
                    stringResource(R.string.app_profile_template_create)
                } else if (readOnly) {
                    stringResource(R.string.app_profile_template_view)
                } else {
                    stringResource(R.string.app_profile_template_edit)
                },
                readOnly = readOnly,
                summary = titleSummary,
                onBack = { navigator.navigateBack(result = !readOnly) },
                onDelete = {
                    if (deleteAppProfileTemplate(template.id)) {
                        navigator.navigateBack(result = true)
                    }
                },
                onSave = {
                    if (saveTemplate(template, isCreation)) {
                        navigator.navigateBack(result = true)
                    } else {
                        Toast.makeText(context, saveTemplateFailed, Toast.LENGTH_SHORT).show()
                    }
                })
        },
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .verticalScroll(rememberScrollState())
                .pointerInteropFilter {
                    // disable click and ripple if readOnly
                    readOnly
                }
        ) {
            if (isCreation) {
                var errorHint by remember {
                    mutableStateOf("")
                }
                val idConflictError = stringResource(id = R.string.app_profile_template_id_exist)
                val idInvalidError = stringResource(id = R.string.app_profile_template_id_invalid)
                TextEdit(
                    label = stringResource(id = R.string.app_profile_template_id),
                    text = template.id,
                    errorHint = errorHint,
                    isError = errorHint.isNotEmpty()
                ) { value ->
                    errorHint = if (isTemplateExist(value)) {
                        idConflictError
                    } else if (!isValidTemplateId(value)) {
                        idInvalidError
                    } else {
                        ""
                    }
                    template = template.copy(id = value)
                }
            }

            TextEdit(
                label = stringResource(id = R.string.app_profile_template_name),
                text = template.name
            ) { value ->
                template.copy(name = value).run {
                    if (autoSave) {
                        if (!saveTemplate(this)) {
                            // failed
                            return@run
                        }
                    }
                    template = this
                }
            }
            TextEdit(
                label = stringResource(id = R.string.app_profile_template_description),
                text = template.description
            ) { value ->
                template.copy(description = value).run {
                    if (autoSave) {
                        if (!saveTemplate(this)) {
                            // failed
                            return@run
                        }
                    }
                    template = this
                }
            }

            RootProfileConfig(fixedName = true,
                profile = toNativeProfile(template),
                onProfileChange = {
                    template.copy(
                        uid = it.uid,
                        gid = it.gid,
                        groups = it.groups,
                        capabilities = it.capabilities,
                        context = it.context,
                        namespace = it.namespace,
                        rules = it.rules.split("\n")
                    ).run {
                        if (autoSave) {
                            if (!saveTemplate(this)) {
                                // failed
                                return@run
                            }
                        }
                        template = this
                    }
                })
        }
    }
}

fun toNativeProfile(templateInfo: TemplateViewModel.TemplateInfo): Natives.Profile {
    return Natives.Profile().copy(rootTemplate = templateInfo.id,
        uid = templateInfo.uid,
        gid = templateInfo.gid,
        groups = templateInfo.groups,
        capabilities = templateInfo.capabilities,
        context = templateInfo.context,
        namespace = templateInfo.namespace,
        rules = templateInfo.rules.joinToString("\n").ifBlank { "" })
}

fun isTemplateValid(template: TemplateViewModel.TemplateInfo): Boolean {
    if (template.id.isBlank()) {
        return false
    }

    if (!isValidTemplateId(template.id)) {
        return false
    }

    return true
}

fun saveTemplate(template: TemplateViewModel.TemplateInfo, isCreation: Boolean = false): Boolean {
    if (!isTemplateValid(template)) {
        return false
    }

    if (isCreation && isTemplateExist(template.id)) {
        return false
    }

    val json = template.toJSON()
    json.put("local", true)
    return setAppProfileTemplate(template.id, json.toString())
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(
    title: String,
    readOnly: Boolean,
    summary: String = "",
    onBack: () -> Unit,
    onDelete: () -> Unit = {},
    onSave: () -> Unit = {}
) {
    TopAppBar(title = {
        Column {
            Text(title)
            if (summary.isNotBlank()) {
                Text(
                    text = summary,
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }
    }, navigationIcon = {
        IconButton(
            onClick = onBack
        ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
    }, actions = {
        if (readOnly) {
            return@TopAppBar
        }
        IconButton(onClick = onDelete) {
            Icon(
                Icons.Filled.DeleteForever,
                contentDescription = stringResource(id = R.string.app_profile_template_delete)
            )
        }
        IconButton(onClick = onSave) {
            Icon(
                imageVector = Icons.Filled.Save,
                contentDescription = stringResource(id = R.string.app_profile_template_save)
            )
        }
    })
}

@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun TextEdit(
    label: String,
    text: String,
    errorHint: String = "",
    isError: Boolean = false,
    onValueChange: (String) -> Unit = {}
) {
    ListItem(headlineContent = {
        val keyboardController = LocalSoftwareKeyboardController.current
        OutlinedTextField(
            value = text,
            modifier = Modifier.fillMaxWidth(),
            label = { Text(label) },
            suffix =
            if (errorHint.isNotBlank()) {
                {
                    Text(
                        text = if (isError) errorHint else "",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error
                    )
                }
            } else {
                null
            },
            isError = isError,
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Ascii, imeAction = ImeAction.Next
            ),
            keyboardActions = KeyboardActions(onDone = {
                keyboardController?.hide()
            }),
            onValueChange = onValueChange
        )
    })
}

private fun isValidTemplateId(id: String): Boolean {
    return Regex("""^([A-Za-z]{1}[A-Za-z\d_]*\.)*[A-Za-z][A-Za-z\d_]*$""").matches(id)
}

private fun isTemplateExist(id: String): Boolean {
    return getAppProfileTemplate(id).isNotBlank()
}