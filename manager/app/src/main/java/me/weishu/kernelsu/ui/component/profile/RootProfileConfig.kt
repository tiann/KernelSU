package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import me.weishu.kernelsu.ui.component.SuperEditArrow
import me.weishu.kernelsu.ui.util.isSepolicyValid
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.extra.CheckboxLocation
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperCheckbox
import top.yukonga.miuix.kmp.extra.SuperDialog
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun RootProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    Column(
        modifier = modifier
    ) {
        if (!fixedName) {
            TextField(
                label = stringResource(R.string.profile_name),
                value = profile.name,
                onValueChange = { onProfileChange(profile.copy(name = it)) }
            )
        }

        SuperEditArrow(
            title = "UID",
            defaultValue = profile.uid,
        ) {
            onProfileChange(
                profile.copy(
                    uid = it,
                    rootUseDefault = false
                )
            )

        }

        SuperEditArrow(
            title = "GID",
            defaultValue = profile.gid,
        ) {
            onProfileChange(
                profile.copy(
                    gid = it,
                    rootUseDefault = false
                )
            )

        }

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

@Composable
fun GroupsPanel(selected: List<Groups>, closeSelection: (selection: Set<Groups>) -> Unit) {
    val showDialog = remember { mutableStateOf(false) }

    val groups = remember {
        Groups.entries.toTypedArray().sortedWith(
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
    }

    val currentSelection = remember { mutableStateOf(selected.toSet()) }

    SuperDialog(
        show = showDialog,
        title = stringResource(R.string.profile_groups),
        summary = "${currentSelection.value.size} / 32",
        insideMargin = DpSize(0.dp, 24.dp),
        onDismissRequest = { showDialog.value = false }
    ) {
        Column(modifier = Modifier.heightIn(max = 500.dp)) {
            LazyColumn(modifier = Modifier.weight(1f, fill = false)) {
                items(groups) { group ->
                    SuperCheckbox(
                        title = group.display,
                        summary = group.desc,
                        insideMargin = PaddingValues(horizontal = 30.dp, vertical = 16.dp),
                        checkboxLocation = CheckboxLocation.Right,
                        checked = currentSelection.value.contains(group),
                        holdDownState = currentSelection.value.contains(group),
                        onCheckedChange = { isChecked ->
                            val newSelection = currentSelection.value.toMutableSet()
                            if (isChecked) {
                                if (newSelection.size < 32) newSelection.add(group)
                            } else {
                                newSelection.remove(group)
                            }
                            currentSelection.value = newSelection
                        }
                    )
                }
            }
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.padding(horizontal = 24.dp),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                TextButton(
                    onClick = {
                        currentSelection.value = selected.toSet()
                        showDialog.value = false
                    },
                    text = stringResource(android.R.string.cancel),
                    modifier = Modifier.weight(1f),
                )
                Spacer(modifier = Modifier.width(20.dp))
                TextButton(
                    onClick = {
                        closeSelection(currentSelection.value)
                        showDialog.value = false
                    },
                    text = stringResource(R.string.confirm),
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.textButtonColorsPrimary()
                )
            }
        }
    }

    val tag = if (selected.isEmpty()) {
        "None"
    } else {
        selected.joinToString(separator = ",", transform = { it.display })
    }
    SuperArrow(
        title = stringResource(R.string.profile_groups),
        summary = tag,
        onClick = {
            showDialog.value = true
        },
    )

}

@Composable
fun CapsPanel(
    selected: Collection<Capabilities>,
    closeSelection: (selection: Set<Capabilities>) -> Unit
) {
    val showDialog = remember { mutableStateOf(false) }

    val caps = remember {
        Capabilities.entries.toTypedArray().sortedWith(
            compareBy<Capabilities> { if (selected.contains(it)) 0 else 1 }
                .then(compareBy { it.name })
        )
    }

    val currentSelection = remember { mutableStateOf(selected.toSet()) }

    SuperDialog(
        show = showDialog,
        title = stringResource(R.string.profile_capabilities),
        insideMargin = DpSize(0.dp, 24.dp),
        onDismissRequest = { showDialog.value = false },
        content = {
            Column(modifier = Modifier.heightIn(max = 500.dp)) {
                LazyColumn(modifier = Modifier.weight(1f, fill = false)) {
                    items(caps) { cap ->
                        SuperCheckbox(
                            title = cap.display,
                            summary = cap.desc,
                            insideMargin = PaddingValues(horizontal = 30.dp, vertical = 16.dp),
                            checkboxLocation = CheckboxLocation.Right,
                            checked = currentSelection.value.contains(cap),
                            holdDownState = currentSelection.value.contains(cap),
                            onCheckedChange = { isChecked ->
                                val newSelection = currentSelection.value.toMutableSet()
                                if (isChecked) {
                                    newSelection.add(cap)
                                } else {
                                    newSelection.remove(cap)
                                }
                                currentSelection.value = newSelection
                            }
                        )
                    }
                }
                Spacer(Modifier.height(12.dp))
                Row(
                    modifier = Modifier.padding(horizontal = 24.dp),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    TextButton(
                        onClick = {
                            showDialog.value = false
                            currentSelection.value = selected.toSet()
                        },
                        text = stringResource(android.R.string.cancel),
                        modifier = Modifier.weight(1f)
                    )
                    Spacer(modifier = Modifier.width(20.dp))
                    TextButton(
                        onClick = {
                            closeSelection(currentSelection.value)
                            showDialog.value = false
                        },
                        text = stringResource(R.string.confirm),
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.textButtonColorsPrimary()
                    )
                }
            }
        }
    )

    val tag = if (selected.isEmpty()) {
        "None"
    } else {
        selected.joinToString(separator = ",", transform = { it.display })
    }
    SuperArrow(
        title = stringResource(R.string.profile_capabilities),
        summary = tag,
        onClick = {
            showDialog.value = true
        }
    )

}

@Composable
private fun SELinuxPanel(
    profile: Natives.Profile,
    onSELinuxChange: (domain: String, rules: String) -> Unit
) {
    val showDialog = remember { mutableStateOf(false) }

    var domain by remember { mutableStateOf(profile.context) }
    var rules by remember { mutableStateOf(profile.rules) }

    val isDomainValid = remember(domain) {
        val regex = Regex("^[a-z_]+:[a-z0-9_]+:[a-z0-9_]+(:[a-z0-9_]+)?$")
        domain.matches(regex)
    }
    val isRulesValid = remember(rules) { isSepolicyValid(rules) }

    SuperDialog(
        show = showDialog,
        title = stringResource(R.string.profile_selinux_context),
        onDismissRequest = { showDialog.value = false }
    ) {
        Column(modifier = Modifier.heightIn(max = 500.dp)) {
            Column(modifier = Modifier.weight(1f, fill = false)) {
                TextField(
                    value = domain,
                    onValueChange = { domain = it },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 8.dp),
                    label = stringResource(id = R.string.profile_selinux_domain),
                    backgroundColor = colorScheme.surfaceContainer,
                    borderColor = if (isDomainValid) {
                        colorScheme.primary
                    } else {
                        Color.Red.copy(alpha = if (isSystemInDarkTheme()) 0.3f else 0.6f)
                    },
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Ascii,
                        imeAction = ImeAction.Next
                    ),
                    singleLine = true
                )
                TextField(
                    value = rules,
                    onValueChange = { rules = it },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 8.dp),
                    label = stringResource(id = R.string.profile_selinux_rules),
                    backgroundColor = colorScheme.surfaceContainer,
                    borderColor = if (isRulesValid) {
                        colorScheme.primary
                    } else {
                        Color.Red.copy(alpha = if (isSystemInDarkTheme()) 0.3f else 0.6f)
                    },
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Ascii,
                    ),
                    singleLine = false
                )
            }
            Spacer(Modifier.height(12.dp))
            Row(
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                TextButton(
                    onClick = { showDialog.value = false },
                    text = stringResource(android.R.string.cancel),
                    modifier = Modifier.weight(1f)
                )
                Spacer(modifier = Modifier.width(20.dp))
                TextButton(
                    onClick = {
                        onSELinuxChange(domain, rules)
                        showDialog.value = false
                    },
                    text = stringResource(R.string.confirm),
                    enabled = isDomainValid && isRulesValid,
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.textButtonColorsPrimary()
                )
            }
        }
    }

    SuperArrow(
        title = stringResource(R.string.profile_selinux_context),
        summary = profile.context,
        onClick = { showDialog.value = true }
    )
}
