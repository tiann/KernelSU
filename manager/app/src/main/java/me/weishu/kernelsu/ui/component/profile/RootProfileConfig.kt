package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.ArrowDropUp
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R

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
            Natives.Profile.Namespace.Inherited -> stringResource(R.string.profile_namespace_inherited)
            Natives.Profile.Namespace.Global -> stringResource(R.string.profile_namespace_global)
            Natives.Profile.Namespace.Individual -> stringResource(R.string.profile_namespace_individual)
        }
        ListItem(headlineContent = {
            ExposedDropdownMenuBox(
                expanded = expanded,
                onExpandedChange = { expanded = !expanded }
            ) {
                OutlinedTextField(
                    modifier = Modifier.menuAnchor(),
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
                            onProfileChange(profile.copy(namespace = Natives.Profile.Namespace.Inherited))
                            expanded = false
                        },
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_global)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = Natives.Profile.Namespace.Global))
                            expanded = false
                        },
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_individual)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = Natives.Profile.Namespace.Individual))
                            expanded = false
                        },
                    )
                }
            }
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("uid") },
                value = profile.uid.toString(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                onValueChange = {
                    if (it.isNotEmpty()) {
                        it.filter { symbol ->
                            symbol.isDigit()
                        }.let { filtered ->
                            filtered.ifEmpty { "0" }
                        }.let { value ->
                            onProfileChange(profile.copy(uid = value.toInt(), rootUseDefault = false))
                        }
                    }
                }

            )
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("gid") },
                value = profile.gid.toString(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                onValueChange = {
                    if (it.isNotEmpty()) {
                        it.filter { symbol ->
                            symbol.isDigit()
                        }.let { filtered ->
                            filtered.ifEmpty { "0" }
                        }.let { value ->
                            onProfileChange(profile.copy(gid = value.toInt(), rootUseDefault = false))
                        }
                    }
                }
            )
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("groups") },
                value = profile.groups.joinToString(","),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                onValueChange = { s ->
                    if (s.isNotEmpty()) {
                        s.filter { symbol ->
                            symbol.isDigit() || symbol == ','
                        }.let { filtered ->
                            filtered.ifEmpty { "0" }
                        }.let { value ->
                            val groups = value.split(',').filter { it.isNotEmpty() }.map { it.toInt() }
                            onProfileChange(profile.copy(groups = groups, rootUseDefault = false))
                        }
                    }
                }
            )
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("context") },
                value = profile.context,
                onValueChange = {
                    onProfileChange(profile.copy(context = it, rootUseDefault = false))
                }
            )
        })
    }
}

@Preview
@Composable
private fun RootProfileConfigPreview() {
    var profile by remember { mutableStateOf(Natives.Profile("")) }
    RootProfileConfig(fixedName = true, profile = profile) {
        profile = it
    }
}
