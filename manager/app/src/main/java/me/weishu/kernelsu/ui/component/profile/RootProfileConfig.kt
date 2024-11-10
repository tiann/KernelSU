package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.ArrowDropUp
import androidx.compose.material3.AssistChip
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.core.text.isDigitsOnly
import com.maxkeppeker.sheets.core.models.base.Header
import com.maxkeppeker.sheets.core.models.base.rememberUseCaseState
import com.maxkeppeler.sheets.input.InputDialog
import com.maxkeppeler.sheets.input.models.InputHeader
import com.maxkeppeler.sheets.input.models.InputSelection
import com.maxkeppeler.sheets.input.models.InputTextField
import com.maxkeppeler.sheets.input.models.InputTextFieldType
import com.maxkeppeler.sheets.input.models.ValidationResult
import com.maxkeppeler.sheets.list.ListDialog
import com.maxkeppeler.sheets.list.models.ListOption
import com.maxkeppeler.sheets.list.models.ListSelection
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import me.weishu.kernelsu.ui.component.rememberCustomDialog
import me.weishu.kernelsu.ui.util.isSepolicyValid

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RootProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    Column(modifier = modifier) {
        if (!fixedName) {
            OutlinedTextField(
                label = { Text(stringResource(R.string.profile_name)) },
                value = profile.name,
                onValueChange = { onProfileChange(profile.copy(name = it)) }
            )
        }

        var expanded by remember { mutableStateOf(false) }
        val currentNamespace = when (profile.namespace) {
            Natives.Profile.Namespace.INHERITED.ordinal -> stringResource(R.string.profile_namespace_inherited)
            Natives.Profile.Namespace.GLOBAL.ordinal -> stringResource(R.string.profile_namespace_global)
            Natives.Profile.Namespace.INDIVIDUAL.ordinal -> stringResource(R.string.profile_namespace_individual)
            else -> stringResource(R.string.profile_namespace_inherited)
        }
        ListItem(headlineContent = {
            ExposedDropdownMenuBox(
                expanded = expanded,
                onExpandedChange = { expanded = !expanded }
            ) {
                OutlinedTextField(
                    modifier = Modifier
                        .menuAnchor(MenuAnchorType.PrimaryNotEditable)
                        .fillMaxWidth(),
                    readOnly = true,
                    label = { Text(stringResource(R.string.profile_namespace)) },
                    value = currentNamespace,
                    onValueChange = {},
                    trailingIcon = {
                        if (expanded) Icon(Icons.Filled.ArrowDropUp, null)
                        else Icon(Icons.Filled.ArrowDropDown, null)
                    },
                )
                ExposedDropdownMenu(
                    expanded = expanded,
                    onDismissRequest = { expanded = false }
                ) {
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_inherited)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = Natives.Profile.Namespace.INHERITED.ordinal))
                            expanded = false
                        },
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_global)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = Natives.Profile.Namespace.GLOBAL.ordinal))
                            expanded = false
                        },
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_individual)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = Natives.Profile.Namespace.INDIVIDUAL.ordinal))
                            expanded = false
                        },
                    )
                }
            }
        })

        UidPanel(uid = profile.uid, label = "uid", onUidChange = {
            onProfileChange(
                profile.copy(
                    uid = it,
                    rootUseDefault = false
                )
            )
        })

        UidPanel(uid = profile.gid, label = "gid", onUidChange = {
            onProfileChange(
                profile.copy(
                    gid = it,
                    rootUseDefault = false
                )
            )
        })

        val selectedGroups = profile.groups.ifEmpty { listOf(0) }.let { e ->
            e.mapNotNull { g ->
                Groups.entries.find { it.gid == g }
            }
        }
        GroupsPanel(selectedGroups) {
            onProfileChange(
                profile.copy(
                    groups = it.map { group -> group.gid }.ifEmpty { listOf(0) },
                    rootUseDefault = false
                )
            )
        }

        val selectedCaps = profile.capabilities.mapNotNull { e ->
            Capabilities.entries.find { it.cap == e }
        }

        CapsPanel(selectedCaps) {
            onProfileChange(
                profile.copy(
                    capabilities = it.map { cap -> cap.cap },
                    rootUseDefault = false
                )
            )
        }

        SELinuxPanel(profile = profile, onSELinuxChange = { domain, rules ->
            onProfileChange(
                profile.copy(
                    context = domain,
                    rules = rules,
                    rootUseDefault = false
                )
            )
        })

    }
}

@OptIn(ExperimentalLayoutApi::class, ExperimentalMaterial3Api::class)
@Composable
fun GroupsPanel(selected: List<Groups>, closeSelection: (selection: Set<Groups>) -> Unit) {
    val selectGroupsDialog = rememberCustomDialog { dismiss: () -> Unit ->
        val groups = Groups.entries.toTypedArray().sortedWith(
            compareBy<Groups> { if (selected.contains(it)) 0 else 1 }
                .then(compareBy {
                    when (it) {
                        Groups.ROOT -> 0
                        Groups.SYSTEM -> 1
                        Groups.SHELL -> 2
                        else -> Int.MAX_VALUE
                    }
                })
                .then(compareBy { it.name })

        )
        val options = groups.map { value ->
            ListOption(
                titleText = value.display,
                subtitleText = value.desc,
                selected = selected.contains(value),
            )
        }

        val selection = HashSet(selected)
        ListDialog(
            state = rememberUseCaseState(visible = true, onFinishedRequest = {
                closeSelection(selection)
            }, onCloseRequest = {
                dismiss()
            }),
            header = Header.Default(
                title = stringResource(R.string.profile_groups),
            ),
            selection = ListSelection.Multiple(
                showCheckBoxes = true,
                options = options,
                maxChoices = 32, // Kernel only supports 32 groups at most
            ) { indecies, _ ->
                // Handle selection
                selection.clear()
                indecies.forEach { index ->
                    val group = groups[index]
                    selection.add(group)
                }
            }
        )
    }

    OutlinedCard(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp)
    ) {

        Column(
            modifier = Modifier
                .fillMaxSize()
                .clickable {
                    selectGroupsDialog.show()
                }
                .padding(16.dp)
        ) {
            Text(stringResource(R.string.profile_groups))
            FlowRow {
                selected.forEach { group ->
                    AssistChip(
                        modifier = Modifier.padding(3.dp),
                        onClick = { /*TODO*/ },
                        label = { Text(group.display) })
                }
            }
        }

    }
}

@OptIn(ExperimentalLayoutApi::class, ExperimentalMaterial3Api::class)
@Composable
fun CapsPanel(
    selected: Collection<Capabilities>,
    closeSelection: (selection: Set<Capabilities>) -> Unit
) {
    val selectCapabilitiesDialog = rememberCustomDialog { dismiss ->
        val caps = Capabilities.entries.toTypedArray().sortedWith(
            compareBy<Capabilities> { if (selected.contains(it)) 0 else 1 }
                .then(compareBy { it.name })
        )
        val options = caps.map { value ->
            ListOption(
                titleText = value.display,
                subtitleText = value.desc,
                selected = selected.contains(value),
            )
        }

        val selection = HashSet(selected)
        ListDialog(
            state = rememberUseCaseState(visible = true, onFinishedRequest = {
                closeSelection(selection)
            }, onCloseRequest = {
                dismiss()
            }),
            header = Header.Default(
                title = stringResource(R.string.profile_capabilities),
            ),
            selection = ListSelection.Multiple(
                showCheckBoxes = true,
                options = options
            ) { indecies, _ ->
                // Handle selection
                selection.clear()
                indecies.forEach { index ->
                    val group = caps[index]
                    selection.add(group)
                }
            }
        )
    }

    OutlinedCard(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp)
    ) {

        Column(
            modifier = Modifier
                .fillMaxSize()
                .clickable {
                    selectCapabilitiesDialog.show()
                }
                .padding(16.dp)
        ) {
            Text(stringResource(R.string.profile_capabilities))
            FlowRow {
                selected.forEach { group ->
                    AssistChip(
                        modifier = Modifier.padding(3.dp),
                        onClick = { /*TODO*/ },
                        label = { Text(group.display) })
                }
            }
        }

    }
}

@Composable
private fun UidPanel(uid: Int, label: String, onUidChange: (Int) -> Unit) {

    ListItem(headlineContent = {
        var isError by remember {
            mutableStateOf(false)
        }
        var lastValidUid by remember {
            mutableIntStateOf(uid)
        }
        val keyboardController = LocalSoftwareKeyboardController.current

        OutlinedTextField(
            modifier = Modifier.fillMaxWidth(),
            label = { Text(label) },
            value = uid.toString(),
            isError = isError,
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Number,
                imeAction = ImeAction.Done
            ),
            keyboardActions = KeyboardActions(onDone = {
                keyboardController?.hide()
            }),
            onValueChange = {
                if (it.isEmpty()) {
                    onUidChange(0)
                    return@OutlinedTextField
                }
                val valid = isTextValidUid(it)

                val targetUid = if (valid) it.toInt() else lastValidUid
                if (valid) {
                    lastValidUid = it.toInt()
                }

                onUidChange(targetUid)

                isError = !valid
            }
        )
    })
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SELinuxPanel(
    profile: Natives.Profile,
    onSELinuxChange: (domain: String, rules: String) -> Unit
) {
    val editSELinuxDialog = rememberCustomDialog { dismiss ->
        var domain by remember { mutableStateOf(profile.context) }
        var rules by remember { mutableStateOf(profile.rules) }

        val inputOptions = listOf(
            InputTextField(
                text = domain,
                header = InputHeader(
                    title = stringResource(id = R.string.profile_selinux_domain),
                ),
                type = InputTextFieldType.OUTLINED,
                required = true,
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Ascii,
                    imeAction = ImeAction.Next
                ),
                resultListener = {
                    domain = it ?: ""
                },
                validationListener = { value ->
                    // value can be a-zA-Z0-9_
                    val regex = Regex("^[a-z_]+:[a-z0-9_]+:[a-z0-9_]+(:[a-z0-9_]+)?$")
                    if (value?.matches(regex) == true) ValidationResult.Valid
                    else ValidationResult.Invalid("Domain must be in the format of \"user:role:type:level\"")
                }
            ),
            InputTextField(
                text = rules,
                header = InputHeader(
                    title = stringResource(id = R.string.profile_selinux_rules),
                ),
                type = InputTextFieldType.OUTLINED,
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Ascii,
                ),
                singleLine = false,
                resultListener = {
                    rules = it ?: ""
                },
                validationListener = { value ->
                    if (isSepolicyValid(value)) ValidationResult.Valid
                    else ValidationResult.Invalid("SELinux rules is invalid!")
                }
            )
        )

        InputDialog(
            state = rememberUseCaseState(visible = true,
                onFinishedRequest = {
                    onSELinuxChange(domain, rules)
                },
                onCloseRequest = {
                    dismiss()
                }),
            header = Header.Default(
                title = stringResource(R.string.profile_selinux_context),
            ),
            selection = InputSelection(
                input = inputOptions,
                onPositiveClick = { result ->
                    // Handle selection
                },
            )
        )
    }

    ListItem(headlineContent = {
        OutlinedTextField(
            modifier = Modifier
                .fillMaxWidth()
                .clickable {
                    editSELinuxDialog.show()
                },
            enabled = false,
            colors = OutlinedTextFieldDefaults.colors(
                disabledTextColor = MaterialTheme.colorScheme.onSurface,
                disabledBorderColor = MaterialTheme.colorScheme.outline,
                disabledPlaceholderColor = MaterialTheme.colorScheme.onSurfaceVariant,
                disabledLabelColor = MaterialTheme.colorScheme.onSurfaceVariant
            ),
            label = { Text(text = stringResource(R.string.profile_selinux_context)) },
            value = profile.context,
            onValueChange = { }
        )
    })
}

@Preview
@Composable
private fun RootProfileConfigPreview() {
    var profile by remember { mutableStateOf(Natives.Profile("")) }
    RootProfileConfig(fixedName = true, profile = profile) {
        profile = it
    }
}

private fun isTextValidUid(text: String): Boolean {
    return text.isNotEmpty() && text.isDigitsOnly() && text.toInt() >= 0 && text.toInt() <= Int.MAX_VALUE
}
