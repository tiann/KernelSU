package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.screen.template.TemplateActions
import me.weishu.kernelsu.ui.screen.template.TemplateUiState
import me.weishu.kernelsu.ui.screen.templateeditor.TemplateEditorActions
import me.weishu.kernelsu.ui.screen.templateeditor.TemplateEditorUiState
import me.weishu.kernelsu.ui.screen.templateeditor.toNativeProfile
import ui.feice.mochi.basic.MochiBarButton
import ui.feice.mochi.basic.MochiBarIconButton
import ui.feice.mochi.basic.MochiBarGroupDropdownItem
import ui.feice.mochi.basic.MochiBarGroupItem
import ui.feice.mochi.basic.MochiBarGroup
import ui.feice.mochi.basic.MochiBarIconDropdown
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiEmptyState
import ui.feice.mochi.basic.MochiItem
import ui.feice.mochi.basic.MochiTextField
import ui.feice.mochi.basic.Text
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListDefaults
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.list.mochiListSection
import ui.feice.mochi.list.rows
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold

@Composable
fun TemplateScreenMochi(state: TemplateUiState, actions: TemplateActions) {
    val refreshing = rememberUpdatedState(state.isRefreshing)
    var menu by remember { mutableStateOf(false) }
    val createLabel = stringResource(R.string.app_profile_template_create)
    val ioLabel = stringResource(R.string.app_profile_import_export)
    MochiLazyLargeTitleScaffold(
        title = stringResource(R.string.settings_profile_template),
        navigationIcon = { MochiBackButton() },
        actions = {
          MochiBarGroup(navId = "template.actions") {
            MochiBarGroupItem(
                onClick = actions.onCreateTemplate,
                modifier = Modifier.semantics { contentDescription = createLabel },
                navId = "template.add",
                contentKey = "add",
            ) { MochiIonicon("add") }
            MochiBarGroupDropdownItem(
                expanded = menu,
                onExpandedChange = { menu = it },
                items = persistentListOf(
                    MochiItem(stringResource(R.string.app_profile_import_from_clipboard), icon = { MochiIonicon("clipboard", tint = it) }),
                    MochiItem(stringResource(R.string.app_profile_export_to_clipboard), icon = { MochiIonicon("copy", tint = it) }),
                ),
                onItemClick = { if (it == 0) actions.onImport() else actions.onExport() },
                modifier = Modifier.semantics { contentDescription = ioLabel },
                navId = "template.io",
                contentKey = "swap",
            ) { MochiIonicon("swap-vertical") }
          }
        },
        onRefresh = { awaitRefresh({ actions.onRefresh(true) }, refreshing) },
    ) {
        if (state.templateList.isEmpty() && !state.isRefreshing) {
            item(key = "empty") {
                if (state.offline) {
                    MochiEmptyState(
                        stringResource(R.string.network_offline),
                        icon = { MochiIonicon("cloud-offline", size = 44.dp) },
                        action = {
                            MochiButton(onClick = { actions.onRefresh(false) }, style = MochiButtonStyle.ProminentGlass) {
                                Text(stringResource(R.string.network_retry))
                            }
                        },
                    )
                } else {
                    MochiEmptyState(createLabel, icon = { MochiIonicon("document-text", size = 44.dp) })
                }
            }
        } else {
            mochiListSection(key = "templates") {
                rows(state.templateList, key = { it.id }) { template ->
                    MochiListRow(
                        template.name.ifEmpty { template.id },
                        subtitle = buildString {
                            append(template.id)
                            if (template.author.isNotEmpty()) append(" by @").append(template.author)
                            if (template.description.isNotBlank()) append('\n').append(template.description)
                        },
                        status = if (template.local) {
                            { MochiListStatusChip("LOCAL") }
                        } else {
                            null
                        },
                        chevron = true,
                        onClick = { actions.onOpenTemplate(template) },
                    )
                }
            }
        }
    }
}

@Composable
fun TemplateEditorScreenMochi(state: TemplateEditorUiState, actions: TemplateEditorActions) {
    val template = state.template
    var confirmDelete by remember { mutableStateOf(false) }
    val deleteLabel = stringResource(R.string.app_profile_template_delete)
    MochiLazyCompactTitleScaffold(
        title = stringResource(
            when {
                state.isCreation -> R.string.app_profile_template_create
                state.readOnly -> R.string.app_profile_template_view
                else -> R.string.app_profile_template_edit
            },
        ),
        subtitle = state.titleSummary.takeIf { !state.isCreation && it.isNotBlank() },
        navigationIcon = { MochiBackButton() },
        actions = {
            when {
                state.isCreation -> MochiBarButton(stringResource(R.string.app_profile_template_save), onClick = actions.onSave, navId = "template.save")
                !state.readOnly -> MochiBarIconButton(
                    onClick = { confirmDelete = true },
                    modifier = Modifier.semantics { contentDescription = deleteLabel },
                    navId = "template.delete",
                    contentKey = "trash",
                ) { MochiIonicon("trash") }
            }
        },
    ) {
        item(key = "info") {
            MochiListSection {
                Column(
                    Modifier.fillMaxWidth().padding(horizontal = MochiListDefaults.RowPadding, vertical = 14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    MochiTextField(
                        value = template.name,
                        onValueChange = actions.onNameChange,
                        placeholder = stringResource(R.string.app_profile_template_name),
                        enabled = !state.readOnly,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    MochiTextField(
                        value = template.id,
                        onValueChange = actions.onIdChange,
                        placeholder = stringResource(R.string.app_profile_template_id),
                        enabled = !state.readOnly && state.isCreation,
                        error = state.idErrorHint.ifEmpty { null },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    MochiTextField(
                        value = template.author,
                        onValueChange = actions.onAuthorChange,
                        placeholder = stringResource(R.string.module_author),
                        enabled = !state.readOnly,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    MochiTextField(
                        value = template.description,
                        onValueChange = actions.onDescriptionChange,
                        placeholder = stringResource(R.string.app_profile_template_description),
                        enabled = !state.readOnly,
                        singleLine = false,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        }
        item(key = "profile") {
            MochiListSection {
                RootProfileConfig(
                    fixedName = true,
                    enabled = !state.readOnly,
                    profile = toNativeProfile(template),
                    onProfileChange = actions.onProfileChange,
                )
            }
        }
    }
    MochiDialog(
        visible = confirmDelete,
        onDismissRequest = { confirmDelete = false },
        title = deleteLabel,
        message = template.name.ifEmpty { template.id },
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(deleteLabel, MochiDialogActionRole.Destructive, onClick = actions.onDelete),
        ),
    )
}
