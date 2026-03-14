package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.SegmentedTextField
import me.weishu.kernelsu.ui.component.profile.dialogs.MultiSelectDialog
import me.weishu.kernelsu.ui.component.profile.dialogs.SingleSelectDialog
import me.weishu.kernelsu.ui.util.isSepolicyValid

@Composable
fun RootProfileConfigMaterial(
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit
) {
    Column(
        modifier = modifier.padding(vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        UidGidPanel(
            enabled = enabled,
            uid = profile.uid,
            gid = profile.gid,
            onUidChange = { onProfileChange(profile.copy(uid = it, rootUseDefault = false)) },
            onGidChange = { onProfileChange(profile.copy(gid = it, rootUseDefault = false)) }
        )

        GroupsPanel(
            enabled = enabled,
            selected = profile.groups.mapNotNull { gid ->
                Groups.entries.find { it.gid == gid }
            },
            onSelectionChange = { selection ->
                onProfileChange(
                    profile.copy(
                        groups = selection.map { it.gid },
                        rootUseDefault = false
                    )
                )
            }
        )

        CapsPanel(
            enabled = enabled,
            selected = profile.capabilities,
            onSelectionChange = { selection ->
                onProfileChange(
                    profile.copy(
                        capabilities = selection.map { it.cap },
                        rootUseDefault = false
                    )
                )
            }
        )

        MountNameSpacePanel(
            enabled = enabled,
            namespace = profile.namespace,
            onNamespaceChange = { onProfileChange(profile.copy(namespace = it, rootUseDefault = false)) }
        )

        SELinuxPanel(
            enabled = enabled,
            context = profile.context,
            rules = profile.rules,
            onContextChange = { domain ->
                onProfileChange(profile.copy(context = domain, rootUseDefault = false))
            },
            onRulesChange = { rules ->
                onProfileChange(profile.copy(rules = rules, rootUseDefault = false))
            }
        )
    }
}

@Composable
private fun UidGidPanel(
    enabled: Boolean,
    uid: Int,
    gid: Int,
    onUidChange: (Int) -> Unit,
    onGidChange: (Int) -> Unit
) {
    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = listOf(
            {
                SegmentedTextField(
                    enabled = enabled,
                    value = uid.toString(),
                    onValueChange = { onUidChange(it.toIntOrNull() ?: 0) },
                    label = "UID",
                    singleLine = true
                )
            },
            {
                SegmentedTextField(
                    enabled = enabled,
                    value = gid.toString(),
                    onValueChange = { onGidChange(it.toIntOrNull() ?: 0) },
                    label = "GID",
                    singleLine = true
                )
            }
        )
    )
}

@Composable
private fun GroupsPanel(
    enabled: Boolean,
    selected: List<Groups>,
    onSelectionChange: (Set<Groups>) -> Unit
) {
    val showDialog = remember { mutableStateOf(false) }

    val groups = remember {
        Groups.entries.sortedWith(
            compareBy<Groups> {
                when (it) {
                    Groups.ROOT -> 0
                    Groups.SYSTEM -> 1
                    Groups.SHELL -> 2
                    else -> Int.MAX_VALUE
                }
            }
                .then(compareBy { it.name })
        )
    }

    if (showDialog.value) {
        MultiSelectDialog(
            title = "Groups",
            subtitle = "${selected.size} / 32",
            items = groups,
            selectedItems = selected.toSet(),
            itemTitle = { it.display },
            itemSubtitle = { it.desc },
            maxSelection = 32,
            onSelectionChange = {
                onSelectionChange(it)
            },
            onDismiss = { showDialog.value = false }
        )
    }

    val tag = if (selected.isEmpty()) {
        "None"
    } else {
        selected.joinToString(", ") { it.display }
    }

    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = listOf {
            SegmentedListItem(
                headlineContent = { Text(stringResource(R.string.profile_groups)) },
                supportingContent = { Text(tag) },
                onClick = if (enabled) {
                    { showDialog.value = true }
                } else null
            )
        }
    )
}

@Composable
private fun MountNameSpacePanel(
    enabled: Boolean,
    namespace: Int,
    onNamespaceChange: (Int) -> Unit
) {
    data class NamespaceOption(
        val value: Int,
        val label: String
    )

    val showDialog = remember { mutableStateOf(false) }

    val inheritedLabel = stringResource(R.string.profile_namespace_inherited)
    val globalLabel = stringResource(R.string.profile_namespace_global)
    val individualLabel = stringResource(R.string.profile_namespace_individual)

    val options = remember(inheritedLabel, globalLabel, individualLabel) {
        listOf(
            NamespaceOption(0, inheritedLabel),
            NamespaceOption(1, globalLabel),
            NamespaceOption(2, individualLabel)
        )
    }

    val selectedOption = options.find { it.value == namespace } ?: options[0]

    if (showDialog.value) {
        SingleSelectDialog(
            title = stringResource(R.string.profile_namespace),
            items = options,
            selectedItem = selectedOption,
            itemTitle = { it.label },
            onConfirm = {
                onNamespaceChange(it.value)
                showDialog.value = false
            },
            onDismiss = { showDialog.value = false }
        )
    }

    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = listOf {
            SegmentedListItem(
                headlineContent = { Text(stringResource(R.string.profile_namespace)) },
                supportingContent = { Text(selectedOption.label) },
                onClick = if (enabled) {
                    { showDialog.value = true }
                } else null
            )
        }
    )
}

@Composable
private fun CapsPanel(
    enabled: Boolean,
    selected: List<Int>,
    onSelectionChange: (Set<Capabilities>) -> Unit
) {
    val showDialog = remember { mutableStateOf(false) }

    val selectedCaps = remember(selected) {
        selected.mapNotNull { cap ->
            Capabilities.entries.find { it.cap == cap }
        }
    }

    val capabilities = remember {
        Capabilities.entries.sortedBy { it.display }
    }

    if (showDialog.value) {
        MultiSelectDialog(
            title = "Capabilities",
            subtitle = "${selectedCaps.size} / ${Capabilities.entries.size}",
            items = capabilities,
            selectedItems = selectedCaps.toSet(),
            itemTitle = { it.display },
            itemSubtitle = { null },
            maxSelection = Int.MAX_VALUE,
            onSelectionChange = {
                onSelectionChange(it)
            },
            onDismiss = { showDialog.value = false }
        )
    }

    val tag = if (selectedCaps.isEmpty()) {
        "None"
    } else {
        selectedCaps.joinToString(", ") { it.display }
    }

    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = listOf {
            SegmentedListItem(
                headlineContent = { Text(stringResource(R.string.profile_capabilities)) },
                supportingContent = { Text(tag) },
                onClick = if (enabled) {
                    { showDialog.value = true }
                } else null
            )
        }
    )
}

@Composable
private fun SELinuxPanel(
    enabled: Boolean,
    context: String,
    rules: String,
    onContextChange: (String) -> Unit,
    onRulesChange: (String) -> Unit
) {
    val showDialog = remember { mutableStateOf(false) }

    if (showDialog.value) {
        SELinuxDialog(
            domain = context,
            rules = rules,
            onConfirm = { domain, r ->
                onContextChange(domain)
                onRulesChange(r)
                showDialog.value = false
            },
            onDismiss = { showDialog.value = false }
        )
    }

    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = listOf {
            SegmentedListItem(
                headlineContent = { Text(stringResource(R.string.profile_selinux_context)) },
                supportingContent = { Text(context.ifEmpty { "—" }) },
                onClick = if (enabled) {
                    { showDialog.value = true }
                } else null
            )
        }
    )
}

@Composable
private fun SELinuxDialog(
    domain: String,
    rules: String,
    onConfirm: (String, String) -> Unit,
    onDismiss: () -> Unit
) {
    var currentDomain by remember { mutableStateOf(domain) }
    var currentRules by remember { mutableStateOf(rules) }

    val isDomainValid = remember(currentDomain) {
        val regex = Regex("^[a-z_]+:[a-z0-9_]+:[a-z0-9_]+(:[a-z0-9_]+)?$")
        currentDomain.matches(regex)
    }
    val isRulesValid = remember(currentRules) { isSepolicyValid(currentRules) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.profile_selinux_context)) },
        text = {
            Column {
                OutlinedTextField(
                    value = currentDomain,
                    onValueChange = { currentDomain = it },
                    label = { Text(stringResource(R.string.profile_selinux_domain)) },
                    isError = !isDomainValid,
                    modifier = Modifier.fillMaxWidth(),
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Ascii,
                        imeAction = ImeAction.Next
                    ),
                    singleLine = true
                )
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(
                    value = currentRules,
                    onValueChange = { currentRules = it },
                    label = { Text(stringResource(R.string.profile_selinux_rules)) },
                    isError = !isRulesValid,
                    modifier = Modifier.fillMaxWidth(),
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Ascii
                    ),
                    minLines = 3,
                    maxLines = 10
                )
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onConfirm(currentDomain, currentRules) },
                enabled = isDomainValid && isRulesValid
            ) {
                Text(stringResource(R.string.confirm))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(android.R.string.cancel))
            }
        }
    )
}
