package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ReadMore
import androidx.compose.material.icons.filled.Create
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.material.ExpressiveColumn
import me.weishu.kernelsu.ui.component.material.ExpressiveListItem
import me.weishu.kernelsu.ui.component.profile.dialogs.SingleSelectDialog
import me.weishu.kernelsu.ui.util.listAppProfileTemplates
import me.weishu.kernelsu.ui.util.setSepolicy
import me.weishu.kernelsu.ui.viewmodel.getTemplateInfoById

private data class TemplateOption(
    val id: String,
    val name: String
)

@Composable
fun TemplateConfigMaterial(
    profile: Natives.Profile,
    onViewTemplate: (id: String) -> Unit = {},
    onManageTemplate: () -> Unit = {},
    onProfileChange: (Natives.Profile) -> Unit
) {
    val showDialog = remember { mutableStateOf(false) }
    val template = rememberSaveable { mutableStateOf(profile.rootTemplate ?: "") }
    val profileTemplates = listAppProfileTemplates()
    val noTemplates = profileTemplates.isEmpty()

    val templateOptions = remember(profileTemplates) {
        profileTemplates.map { tid ->
            TemplateOption(tid, tid)
        }
    }

    val selectedTemplate = remember(template.value, templateOptions) {
        templateOptions.find { it.id == template.value } ?: templateOptions.firstOrNull()
    }

    if (showDialog.value && !noTemplates) {
        SingleSelectDialog(
            title = stringResource(R.string.profile_template),
            items = templateOptions,
            selectedItem = selectedTemplate ?: templateOptions.first(),
            itemTitle = { it.name },
            onConfirm = { selected ->
                val tid = selected.id
                val templateInfo = getTemplateInfoById(tid)
                if (templateInfo != null && setSepolicy(tid, templateInfo.rules.joinToString("\n"))) {
                    onProfileChange(
                        profile.copy(
                            rootTemplate = tid,
                            rootUseDefault = false,
                            uid = templateInfo.uid,
                            gid = templateInfo.gid,
                            groups = templateInfo.groups,
                            capabilities = templateInfo.capabilities,
                            context = templateInfo.context,
                            namespace = templateInfo.namespace,
                        )
                    )
                    template.value = tid
                }
                showDialog.value = false
            },
            onDismiss = { showDialog.value = false }
        )
    }

    val selectedTemplateName = template.value.ifEmpty { "None" }

    ExpressiveColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = listOf(
            {
                ExpressiveListItem(
                    headlineContent = { Text(stringResource(R.string.profile_template)) },
                    supportingContent = { Text(selectedTemplateName) },
                    trailingContent = {
                        if (noTemplates) {
                            IconButton(onClick = onManageTemplate) {
                                Icon(Icons.Filled.Create, contentDescription = null)
                            }
                        }
                    },
                    onClick = {
                        if (!noTemplates) {
                            showDialog.value = true
                        }
                    }
                )
            },
            {
                if (template.value.isNotEmpty()) {
                    ExpressiveListItem(
                        headlineContent = { Text(stringResource(R.string.app_profile_template_view)) },
                        trailingContent = {
                            Icon(Icons.AutoMirrored.Filled.ReadMore, contentDescription = null)
                        },
                        onClick = { onViewTemplate(template.value) }
                    )
                }
            }
        )
    )
}
