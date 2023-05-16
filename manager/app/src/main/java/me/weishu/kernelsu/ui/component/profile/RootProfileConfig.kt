package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
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
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.RootProfile

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RootProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    profile: RootProfile,
    onProfileChange: (RootProfile) -> Unit,
) {
    Column(modifier = modifier) {
        if (!fixedName) {
            OutlinedTextField(
                label = { Text(stringResource(R.string.profile_name)) },
                value = profile.profileName,
                onValueChange = { onProfileChange(profile.copy(profileName = it)) }
            )
        }

        var namespaceBoxExpanded by remember { mutableStateOf(false) }
        val currentNamespace = when (profile.namespace) {
            RootProfile.Namespace.Inherited -> stringResource(R.string.profile_namespace_inherited)
            RootProfile.Namespace.Global -> stringResource(R.string.profile_namespace_global)
            RootProfile.Namespace.Individual -> stringResource(R.string.profile_namespace_individual)
        }
        ListItem(headlineContent = {
            ExposedDropdownMenuBox(
                expanded = namespaceBoxExpanded,
                onExpandedChange = { namespaceBoxExpanded = it }
            ) {
                OutlinedTextField(
                    modifier = Modifier.menuAnchor(),
                    readOnly = true,
                    label = { Text(stringResource(R.string.profile_namespace)) },
                    value = currentNamespace,
                    onValueChange = {},
                )
                ExposedDropdownMenu(
                    expanded = namespaceBoxExpanded,
                    onDismissRequest = { namespaceBoxExpanded = false }
                ) {
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_inherited)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = RootProfile.Namespace.Inherited))
                            namespaceBoxExpanded = false
                        },
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_global)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = RootProfile.Namespace.Global))
                            namespaceBoxExpanded = false
                        },
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.profile_namespace_individual)) },
                        onClick = {
                            onProfileChange(profile.copy(namespace = RootProfile.Namespace.Individual))
                            namespaceBoxExpanded = false
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
                onValueChange = { onProfileChange(profile.copy(uid = it.toInt())) }
            )
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("gid") },
                value = profile.gid.toString(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                onValueChange = { onProfileChange(profile.copy(gid = it.toInt())) }
            )
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("groups") },
                value = profile.groups.toString(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                onValueChange = { onProfileChange(profile.copy(groups = it.toInt())) }
            )
        })

        ListItem(headlineContent = {
            OutlinedTextField(
                label = { Text("context") },
                value = profile.context,
                onValueChange = { onProfileChange(profile.copy(context = it)) }
            )
        })
    }
}

@Preview
@Composable
private fun RootProfileConfigPreview() {
    var profile by remember { mutableStateOf(RootProfile("")) }
    RootProfileConfig(fixedName = true, profile = profile) {
        profile = it
    }
}
