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
import me.weishu.kernelsu.R
import me.weishu.kernelsu.profile.AppProfile
import me.weishu.kernelsu.ui.component.SwitchItem

@Composable
fun AppProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    profile: AppProfile,
    onProfileChange: (AppProfile) -> Unit,
) {
    Column(modifier = modifier) {
        if (!fixedName) {
            OutlinedTextField(
                label = { Text(stringResource(R.string.profile_name)) },
                value = profile.profileName,
                onValueChange = { onProfileChange(profile.copy(profileName = it)) }
            )
        }

        SwitchItem(
            title = stringResource(R.string.profile_allow_root_request),
            checked = profile.allowRootRequest,
            onCheckedChange = { onProfileChange(profile.copy(allowRootRequest = it)) }
        )

        SwitchItem(
            title = stringResource(R.string.profile_unmount_modules),
            checked = profile.unmountModules,
            onCheckedChange = { onProfileChange(profile.copy(unmountModules = it)) }
        )
    }
}

@Preview
@Composable
private fun AppProfileConfigPreview() {
    var profile by remember { mutableStateOf(AppProfile("")) }
    AppProfileConfig(fixedName = true, profile = profile) {
        profile = it
    }
}
