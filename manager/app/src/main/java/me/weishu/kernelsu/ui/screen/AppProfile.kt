@file:OptIn(ExperimentalMaterial3Api::class)

package me.weishu.kernelsu.ui.screen

import android.content.pm.PackageInfo
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.DeveloperMode
import androidx.compose.material.icons.filled.Group
import androidx.compose.material.icons.filled.Groups
import androidx.compose.material.icons.filled.Groups2
import androidx.compose.material.icons.filled.Groups3
import androidx.compose.material.icons.filled.List
import androidx.compose.material.icons.filled.PermDeviceInformation
import androidx.compose.material.icons.filled.PermIdentity
import androidx.compose.material.icons.filled.Rule
import androidx.compose.material.icons.filled.RuleFolder
import androidx.compose.material.icons.filled.SafetyDivider
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.Divider
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import coil.compose.AsyncImage
import coil.request.ImageRequest
import com.alorma.compose.settings.ui.SettingsGroup
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AboutDialog
import me.weishu.kernelsu.ui.component.ConfirmResult
import me.weishu.kernelsu.ui.component.LoadingDialog
import me.weishu.kernelsu.ui.util.LocalDialogHost
import me.weishu.kernelsu.ui.util.LocalSnackbarHost

/**
 * @author weishu
 * @date 2023/5/16.
 */
@Destination
@Composable
fun AppProfileScreen(
    navigator: DestinationsNavigator,
    packageName: String,
    grantRoot: Boolean,
    label: String,
    icon: PackageInfo
) {

    val snackbarHost = LocalSnackbarHost.current

    Scaffold(
        topBar = {
            TopBar(onBack = {
                navigator.popBackStack()
            })
        }
    ) { paddingValues ->
        LoadingDialog()

        val showAboutDialog = remember { mutableStateOf(false) }
        AboutDialog(showAboutDialog)

        Column(modifier = Modifier.padding(paddingValues)) {

            val context = LocalContext.current
            val scope = rememberCoroutineScope()

            GroupTitle(stringResource(id = R.string.app_profile_title1))

            ListItem(
                headlineText = { Text(label) },
                supportingText = { Text(packageName) },
                leadingContent = {
                    AsyncImage(
                        model = ImageRequest.Builder(LocalContext.current)
                            .data(icon)
                            .crossfade(true)
                            .build(),
                        contentDescription = label,
                        modifier = Modifier
                            .padding(4.dp)
                            .width(48.dp)
                            .height(48.dp)
                    )
                },
            )

            var isChecked by rememberSaveable {
                mutableStateOf(grantRoot)
            }

            val failMessage = stringResource(R.string.superuser_failed_to_grant_root)
            val uid = icon.applicationInfo.uid;
            AppSwitch(
                Icons.Filled.Security,
                stringResource(id = R.string.app_profile_root_switch),
                checked = isChecked
            ) { checked ->

                scope.launch {
                    val success = Natives.allowRoot(uid, checked)
                    if (success) {
                        isChecked = checked
                    } else {
                        snackbarHost.showSnackbar(failMessage.format(uid))
                    }
                }
            }

            AppSwitch(
                icon = Icons.Filled.List,
                title = stringResource(id = R.string.app_profile_allowlist),
                checked = true
            ) {

            }

            Divider(thickness = Dp.Hairline)

            GroupTitle(title = stringResource(id = R.string.app_profile_title2))

            Uid()

            Gid()

            Groups()

            Capabilities()

            SELinuxDomain()
        }
    }
}

@Composable
private fun GroupTitle(title: String) {
    Row(modifier = Modifier.padding(12.dp)) {
        Spacer(modifier = Modifier.width(30.dp))
        Text(
            text = title,
            color = MaterialTheme.colorScheme.primary,
            fontStyle = MaterialTheme.typography.titleSmall.fontStyle,
            fontSize = MaterialTheme.typography.titleSmall.fontSize,
            fontWeight = MaterialTheme.typography.titleSmall.fontWeight,
        )
    }
}

@Composable
private fun AppSwitch(
    icon: ImageVector,
    title: String,
    checked: Boolean,
    onCheckChange: (Boolean) -> Unit
) {
    ListItem(
        headlineText = { Text(title) },
        leadingContent = {
            Icon(
                icon,
                contentDescription = title
            )
        },
        trailingContent = {
            Switch(checked = checked, onCheckedChange = onCheckChange)
        }
    )
}

@Composable
private fun Uid() {
    ListItem(
        headlineText = {
            Text("Uid: 0")
        },
        leadingContent = {
            Icon(
                Icons.Filled.PermIdentity,
                contentDescription = "Uid"
            )
        },
    )
}

@Composable
private fun Gid() {
    ListItem(
        headlineText = { Text("Gid: 0") },
        leadingContent = {
            Icon(
                Icons.Filled.Group,
                contentDescription = "Gid"
            )
        },
    )
}

@Composable
private fun Groups() {
    ListItem(
        headlineText = { Text("Groups: 0") },
        leadingContent = {
            Icon(
                Icons.Filled.Groups3,
                contentDescription = "Groups"
            )
        },
    )
}

@Composable
private fun Capabilities() {
    ListItem(
        headlineText = { Text("Capabilities") },
        leadingContent = {
            Icon(
                Icons.Filled.SafetyDivider,
                contentDescription = "Capabilities"
            )
        },
    )
}

@Composable
private fun SELinuxDomain() {
    ListItem(
        headlineText = { Text("u:r:su:s0") },
        leadingContent = {
            Icon(
                Icons.Filled.Rule,
                contentDescription = "SELinuxDomain"
            )
        },
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit = {}) {
    TopAppBar(
        title = {
            Text(stringResource(R.string.app_profile))
        },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
        },
    )
}