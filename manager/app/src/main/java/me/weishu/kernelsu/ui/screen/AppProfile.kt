package me.weishu.kernelsu.ui.screen

import android.util.Log
import androidx.annotation.StringRes
import androidx.compose.animation.Crossfade
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AccountCircle
import androidx.compose.material.icons.filled.Android
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.ArrowDropUp
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.Divider
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import coil.compose.AsyncImage
import coil.request.ImageRequest
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.SwitchItem
import me.weishu.kernelsu.ui.component.profile.AppProfileConfig
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel

/**
 * @author weishu
 * @date 2023/5/16.
 */
@Destination
@Composable
fun AppProfileScreen(
    navigator: DestinationsNavigator,
    appInfo: SuperUserViewModel.AppInfo,
) {
    val context = LocalContext.current
    val snackbarHost = LocalSnackbarHost.current
    val scope = rememberCoroutineScope()
    val failToUpdateAppProfile =
        stringResource(R.string.failed_to_update_app_profile).format(appInfo.label)

    val packageName = appInfo.packageName
    var profile by rememberSaveable {
        mutableStateOf(Natives.getAppProfile(packageName, appInfo.uid))
    }

    Log.i("mylog", "profile: $profile")

    Scaffold(
        topBar = { TopBar { navigator.popBackStack() } }
    ) { paddingValues ->
        AppProfileInner(
            modifier = Modifier
                .padding(paddingValues)
                .verticalScroll(rememberScrollState()),
            packageName = appInfo.packageName,
            appLabel = appInfo.label,
            appIcon = {
                AsyncImage(
                    model = ImageRequest.Builder(context)
                        .data(appInfo.packageInfo)
                        .crossfade(true)
                        .build(),
                    contentDescription = appInfo.label,
                    modifier = Modifier
                        .padding(4.dp)
                        .width(48.dp)
                        .height(48.dp)
                )
            },
            profile = profile,
            onProfileChange = {
                scope.launch {
                    if (!Natives.setAppProfile(it)) {
                        snackbarHost.showSnackbar(failToUpdateAppProfile.format(appInfo.uid))
                    } else {
                        profile = it
                    }
                }
            },
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AppProfileInner(
    modifier: Modifier = Modifier,
    packageName: String,
    appLabel: String,
    appIcon: @Composable () -> Unit,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    val isRootGranted = profile.allowSu

    Column(modifier = modifier) {
        ListItem(
            headlineContent = { Text(appLabel) },
            supportingContent = { Text(packageName) },
            leadingContent = appIcon,
        )

        SwitchItem(
            icon = Icons.Filled.Security,
            title = stringResource(id = R.string.superuser),
            checked = isRootGranted,
            onCheckedChange = { onProfileChange(profile.copy(allowSu = it)) },
        )

        Crossfade(targetState = isRootGranted, label = "") { current ->
            Column {
                if (current) {
                    val initialMode = if (profile.rootUseDefault) {
                        Mode.Default
                    } else if (profile.rootTemplate != null) {
                        Mode.Template
                    } else {
                        Mode.Custom
                    }
                    var mode by remember {
                        mutableStateOf(initialMode)
                    }
                    ProfileBox(mode, false) {
                        // template mode shouldn't change profile here!
                        if (it == Mode.Default || it == Mode.Custom) {
                            onProfileChange(profile.copy(rootUseDefault = it == Mode.Default))
                        }
                        mode = it
                    }
                    Crossfade(targetState = mode, label = "") { currentMode ->
                        if (currentMode == Mode.Template) {
                            var expanded by remember { mutableStateOf(false) }
                            val templateNone = "None"
                            var template by rememberSaveable {
                                mutableStateOf(
                                    profile.rootTemplate
                                        ?: templateNone
                                )
                            }
                            ListItem(headlineContent = {
                                ExposedDropdownMenuBox(
                                    expanded = expanded,
                                    onExpandedChange = { expanded = it },
                                ) {
                                    OutlinedTextField(
                                        modifier = Modifier.menuAnchor(),
                                        readOnly = true,
                                        label = { Text(stringResource(R.string.profile_template)) },
                                        value = template,
                                        onValueChange = {
                                            if (template != templateNone) {
                                                onProfileChange(
                                                    profile.copy(
                                                        rootTemplate = it,
                                                        rootUseDefault = false
                                                    )
                                                )
                                                template = it
                                            }
                                        },
                                        trailingIcon = {
                                            if (expanded) Icon(Icons.Filled.ArrowDropUp, null)
                                            else Icon(Icons.Filled.ArrowDropDown, null)
                                        },
                                    )
                                    // TODO: Template
                                }
                            })
                        } else if (mode == Mode.Custom) {
                            RootProfileConfig(
                                fixedName = true,
                                profile = profile,
                                onProfileChange = onProfileChange
                            )
                        }
                    }
                } else {
                    val mode = if (profile.nonRootUseDefault) Mode.Default else Mode.Custom
                    ProfileBox(mode, false) {
                        onProfileChange(profile.copy(nonRootUseDefault = (it == Mode.Default)))
                    }
                    Crossfade(targetState = mode, label = "") { currentMode ->
                        val modifyEnabled = currentMode == Mode.Custom
                        AppProfileConfig(
                            fixedName = true,
                            profile = profile,
                            enabled = modifyEnabled,
                            onProfileChange = onProfileChange
                        )
                    }
                }
            }
        }
    }
}

private enum class Mode(@StringRes private val res: Int) {
    Default(R.string.profile_default),
    Template(R.string.profile_template),
    Custom(R.string.profile_custom);

    val text: String
        @Composable get() = stringResource(res)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit) {
    TopAppBar(
        title = {
            Text(stringResource(R.string.profile))
        },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
        },
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ProfileBox(
    mode: Mode,
    hasTemplate: Boolean,
    onModeChange: (Mode) -> Unit,
) {
    ListItem(
        headlineContent = { Text(stringResource(R.string.profile)) },
        supportingContent = { Text(mode.text) },
        leadingContent = { Icon(Icons.Filled.AccountCircle, null) },
    )
    Divider(thickness = Dp.Hairline)
    ListItem(headlineContent = {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            FilterChip(
                selected = mode == Mode.Default,
                label = { Text(stringResource(R.string.profile_default)) },
                onClick = { onModeChange(Mode.Default) },
            )
            if (hasTemplate) {
                FilterChip(
                    selected = mode == Mode.Template,
                    label = { Text(stringResource(R.string.profile_template)) },
                    onClick = { onModeChange(Mode.Template) },
                )
            }
            FilterChip(
                selected = mode == Mode.Custom,
                label = { Text(stringResource(R.string.profile_custom)) },
                onClick = { onModeChange(Mode.Custom) },
            )
        }
    })
}

@Preview
@Composable
private fun AppProfilePreview() {
    var profile by remember { mutableStateOf(Natives.Profile("")) }
    AppProfileInner(
        packageName = "icu.nullptr.test",
        appLabel = "Test",
        appIcon = { Icon(Icons.Filled.Android, null) },
        profile = profile,
        onProfileChange = {
            profile = it
        },
    )
}

