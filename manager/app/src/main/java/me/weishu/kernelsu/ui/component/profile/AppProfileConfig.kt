package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.layout.Column
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.SwitchItem

@Composable
fun AppProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    enabled: Boolean,
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

        SwitchItem(
            title = stringResource(R.string.profile_umount_modules),
            summary = stringResource(R.string.profile_umount_modules_summary),
            checked = if (enabled) {
                profile.umountModules
            } else {
                Natives.isDefaultUmountModules()
            },
            enabled = enabled,
            onCheckedChange = {
                onProfileChange(
                    profile.copy(
                        umountModules = it,
                        nonRootUseDefault = false
                    )
                )
            }
        )
    }
}

@Preview
@Composable
private fun AppProfileConfigPreview() {
    var profile by remember { mutableStateOf(Natives.Profile("")) }
    AppProfileConfig(fixedName = true, enabled = false, profile = profile) {
        profile = it
    }
}
