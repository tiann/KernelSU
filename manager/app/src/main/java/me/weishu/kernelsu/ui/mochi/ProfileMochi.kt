package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import me.weishu.kernelsu.toRawFlags
import me.weishu.kernelsu.toRootProfileFlags
import me.weishu.kernelsu.ui.util.isSepolicyValid
import me.weishu.kernelsu.ui.util.listAppProfileTemplates
import me.weishu.kernelsu.ui.util.setSepolicy
import me.weishu.kernelsu.ui.viewmodel.getTemplateInfoById
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogScrollContent
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiSwitch
import ui.feice.mochi.basic.MochiTextField
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow

/*
 * App profile config rows. The caller wraps them in a MochiListSection.
 */

@Composable
fun RootProfileConfigMochi(
    fixedName: Boolean,
    enabled: Boolean,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    if (!fixedName) {
        TextEditRow(stringResource(R.string.profile_name), profile.name, enabled) { onProfileChange(profile.copy(name = it)) }
    }
    NumberEditRow("UID", profile.uid, enabled) { onProfileChange(profile.copy(uid = it, rootUseDefault = false)) }
    NumberEditRow("GID", profile.gid, enabled) { onProfileChange(profile.copy(gid = it, rootUseDefault = false)) }

    val groups = remember {
        Groups.entries.sortedWith(
            compareBy<Groups> {
                when (it) {
                    Groups.ROOT -> 0
                    Groups.SYSTEM -> 1
                    Groups.SHELL -> 2
                    else -> Int.MAX_VALUE
                }
            }.then(compareBy { it.name }),
        )
    }
    val selectedGroups = profile.groups.mapNotNull { g -> Groups.entries.find { it.gid == g } }.toSet()
    MultiSelectRow(
        title = stringResource(R.string.profile_groups),
        enabled = enabled,
        options = groups,
        selected = selectedGroups,
        label = { it.display },
        description = { it.desc },
        limit = 32,
    ) { onProfileChange(profile.copy(groups = it.map { g -> g.gid }, rootUseDefault = false)) }

    val caps = remember { Capabilities.entries.sortedBy { it.name } }
    val selectedCaps = profile.capabilities.mapNotNull { c -> Capabilities.entries.find { it.cap == c } }.toSet()
    MultiSelectRow(
        title = stringResource(R.string.profile_capabilities),
        enabled = enabled,
        options = caps,
        selected = selectedCaps,
        label = { it.display },
        description = { it.desc },
    ) { onProfileChange(profile.copy(capabilities = it.map { c -> c.cap }, rootUseDefault = false)) }

    MochiListPickerRow(
        stringResource(R.string.profile_namespace),
        options = persistentListOf(
            stringResource(R.string.profile_namespace_inherited),
            stringResource(R.string.profile_namespace_global),
            stringResource(R.string.profile_namespace_individual),
        ),
        selectedIndex = profile.namespace.coerceIn(0, 2),
        onSelect = { onProfileChange(profile.copy(namespace = it, rootUseDefault = false)) },
        enabled = enabled,
    )

    val flags = remember { Natives.Profile.RootProfileFlag.entries.sortedBy { it.display } }
    val flagDescriptions = flags.associateWith { stringResource(it.desc) }
    MultiSelectRow(
        title = stringResource(R.string.profile_flags),
        enabled = enabled,
        options = flags,
        selected = profile.flags.toRootProfileFlags().toSet(),
        label = { it.display },
        description = { flagDescriptions[it].orEmpty() },
    ) { onProfileChange(profile.copy(flags = it.toList().toRawFlags())) }

    SELinuxRow(enabled, profile) { domain, rules ->
        onProfileChange(profile.copy(context = domain, rules = rules, rootUseDefault = false))
    }
}

@Composable
fun AppProfileConfigMochi(
    fixedName: Boolean,
    enabled: Boolean,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    if (!fixedName) {
        TextEditRow(stringResource(R.string.profile_name), profile.name, enabled) { onProfileChange(profile.copy(name = it)) }
    }
    MochiListRow(
        stringResource(R.string.profile_umount_modules),
        subtitle = stringResource(R.string.profile_umount_modules_summary),
        enabled = enabled,
        trailing = {
            MochiSwitch(
                if (enabled) profile.umountModules else Natives.isDefaultUmountModules(),
                { onProfileChange(profile.copy(umountModules = it, nonRootUseDefault = false)) },
                enabled = enabled,
            )
        },
    )
}

@Composable
fun TemplateConfigMochi(
    profile: Natives.Profile,
    onViewTemplate: (String) -> Unit,
    onManageTemplate: () -> Unit,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    val templates = remember { listAppProfileTemplates() }
    if (templates.isEmpty()) {
        MochiListRow(
            stringResource(R.string.app_profile_template_create),
            leading = { KsuTile("add", KsuTileColors.Orange) },
            chevron = true,
            onClick = onManageTemplate,
        )
        return
    }
    val template = profile.rootTemplate ?: templates[0]
    MochiListPickerRow(
        stringResource(R.string.profile_template),
        options = templates.toImmutableList(),
        selectedIndex = templates.indexOf(template).coerceAtLeast(0),
        onSelect = { index ->
            val selected = templates.getOrNull(index) ?: return@MochiListPickerRow
            val info = getTemplateInfoById(selected)
            if (info != null && setSepolicy(selected, info.rules.joinToString("\n"))) {
                onProfileChange(
                    profile.copy(
                        rootTemplate = selected,
                        rootUseDefault = false,
                        uid = info.uid,
                        gid = info.gid,
                        groups = info.groups,
                        capabilities = info.capabilities,
                        context = info.context,
                        namespace = info.namespace,
                    ),
                )
            }
        },
    )
    MochiListRow(stringResource(R.string.app_profile_template_view), chevron = true, onClick = { onViewTemplate(template) })
}

@Composable
internal fun TextEditRow(title: String, value: String, enabled: Boolean, onChange: (String) -> Unit) {
    var editing by remember { mutableStateOf(false) }
    var draft by remember(value) { mutableStateOf(value) }
    MochiListRow(title, value = value.ifEmpty { null }, enabled = enabled, chevron = true, onClick = { editing = true })
    MochiDialog(
        visible = editing,
        onDismissRequest = {
            editing = false
            draft = value
        },
        title = title,
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(stringResource(R.string.confirm), MochiDialogActionRole.Primary, onClick = { onChange(draft) }),
        ),
    ) {
        MochiTextField(value = draft, onValueChange = { draft = it }, modifier = Modifier.fillMaxWidth())
    }
}

@Composable
private fun NumberEditRow(title: String, value: Int, enabled: Boolean, onChange: (Int) -> Unit) {
    var editing by remember { mutableStateOf(false) }
    var draft by remember(value) { mutableStateOf(value.toString()) }
    val parsed = draft.toIntOrNull()
    MochiListRow(title, value = value.toString(), enabled = enabled, chevron = true, onClick = { editing = true })
    MochiDialog(
        visible = editing,
        onDismissRequest = {
            editing = false
            draft = value.toString()
        },
        title = title,
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(
                stringResource(R.string.confirm),
                MochiDialogActionRole.Primary,
                enabled = parsed != null && parsed >= 0,
                onClick = { parsed?.let(onChange) },
            ),
        ),
    ) {
        MochiTextField(
            value = draft,
            onValueChange = { draft = it.filter(Char::isDigit) },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun <T> MultiSelectRow(
    title: String,
    enabled: Boolean,
    options: List<T>,
    selected: Set<T>,
    label: (T) -> String,
    description: (T) -> String,
    limit: Int = Int.MAX_VALUE,
    onConfirm: (Set<T>) -> Unit,
) {
    var open by remember { mutableStateOf(false) }
    var draft by remember(selected) { mutableStateOf(selected) }
    MochiListRow(
        title,
        subtitle = if (selected.isEmpty()) "None" else selected.joinToString(", ") { label(it) },
        enabled = enabled,
        chevron = true,
        onClick = { open = true },
    )
    MochiDialog(
        visible = open,
        onDismissRequest = {
            open = false
            draft = selected
        },
        title = title,
        message = if (limit != Int.MAX_VALUE) "${draft.size} / $limit" else null,
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(stringResource(R.string.confirm), MochiDialogActionRole.Primary, onClick = { onConfirm(draft) }),
        ),
    ) {
        MochiDialogScrollContent(maxHeight = 380.dp) {
            MochiListCard {
                options.forEach { option ->
                    val on = option in draft
                    MochiListRow(
                        label(option),
                        subtitle = description(option).ifBlank { null },
                        checked = on,
                        onClick = {
                            draft = if (on) draft - option else if (draft.size < limit) draft + option else draft
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun SELinuxRow(enabled: Boolean, profile: Natives.Profile, onChange: (String, String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    var domain by remember(profile.context) { mutableStateOf(profile.context) }
    var rules by remember(profile.rules) { mutableStateOf(profile.rules) }
    val domainValid = remember(domain) { domain.matches(Regex("^[a-z_]+:[a-z0-9_]+:[a-z0-9_]+(:[a-z0-9_]+)?$")) }
    val rulesValid = remember(rules) { isSepolicyValid(rules) }
    val title = stringResource(R.string.profile_selinux_context)
    MochiListRow(title, subtitle = profile.context, enabled = enabled, chevron = true, onClick = { open = true })
    MochiDialog(
        visible = open,
        onDismissRequest = { open = false },
        title = title,
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(
                stringResource(R.string.confirm),
                MochiDialogActionRole.Primary,
                enabled = domainValid && rulesValid,
                onClick = { onChange(domain, rules) },
            ),
        ),
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            MochiTextField(
                value = domain,
                onValueChange = { domain = it },
                placeholder = stringResource(R.string.profile_selinux_domain),
                error = if (domainValid) null else "",
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii),
                modifier = Modifier.fillMaxWidth(),
            )
            MochiTextField(
                value = rules,
                onValueChange = { rules = it },
                placeholder = stringResource(R.string.profile_selinux_rules),
                error = if (rulesValid) null else "",
                singleLine = false,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
