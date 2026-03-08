package me.weishu.kernelsu.ui.screen.appprofile

import android.os.Build
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.AccountCircle
import androidx.compose.material.icons.filled.Android
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.ButtonGroupDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.Text
import androidx.compose.material3.ToggleButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.SegmentedSwitchItem
import me.weishu.kernelsu.ui.component.profile.AppProfileConfig
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.component.profile.TemplateConfig
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.util.forceStopApp
import me.weishu.kernelsu.ui.util.getSepolicy
import me.weishu.kernelsu.ui.util.launchApp
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.util.restartApp
import me.weishu.kernelsu.ui.util.setSepolicy
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import me.weishu.kernelsu.ui.viewmodel.getTemplateInfoById

/**
 * @author weishu
 * @date 2023/5/16.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppProfileScreenMaterial(
    uid: Int,
    packageName: String,
) {
    val viewModel: SuperUserViewModel = viewModel()
    val navigator = LocalNavigator.current
    val snackBarHost = LocalSnackbarHost.current
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val scope = rememberCoroutineScope()
    val appInfoState = remember(uid, packageName) {
        derivedStateOf { SuperUserViewModel.apps.find { it.uid == uid && it.packageName == packageName } }
    }
    val appInfo = appInfoState.value
    if (appInfo == null) {
        LaunchedEffect(Unit) {
            navigator.pop()
        }
        return
    }
    val failToUpdateAppProfile = stringResource(R.string.failed_to_update_app_profile).format(appInfo.label)
    val failToUpdateSepolicy = stringResource(R.string.failed_to_update_sepolicy).format(appInfo.label)
    val suNotAllowed = stringResource(R.string.su_not_allowed).format(appInfo.label)
    val sameUidApps = remember(uid) {
        SuperUserViewModel.apps.filter { it.uid == uid }
    }
    val isUidGroup = sameUidApps.size > 1
    // The package name from the SuperUser is the primary package, so no need to recalculate.
    val sharedUserId = remember(sameUidApps, appInfo) {
        appInfo.packageInfo.sharedUserId
            ?: sameUidApps.firstOrNull { it.packageInfo.sharedUserId != null }?.packageInfo?.sharedUserId
            ?: ""
    }
    val initialProfile = Natives.getAppProfile(packageName, uid)
    if (initialProfile.allowSu) {
        initialProfile.rules = getSepolicy(packageName)
    }
    var profile by rememberSaveable {
        mutableStateOf(initialProfile)
    }

    LaunchedEffect(Unit) {
        scrollBehavior.state.heightOffset = scrollBehavior.state.heightOffsetLimit
    }

    Scaffold(
        topBar = {
            TopBar(
                onBack = dropUnlessResumed { navigator.pop() },
                scrollBehavior = scrollBehavior,
                isUidGroup = isUidGroup,
                packageName = appInfo.packageName
            )
        },
        snackbarHost = { SnackbarHost(hostState = snackBarHost) },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { paddingValues ->
        AppProfileInner(
            modifier = Modifier
                .padding(paddingValues)
                .fillMaxHeight()
                .imePadding()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState()),
            packageName = if (isUidGroup) "" else appInfo.packageName,
            appLabel = if (isUidGroup) ownerNameForUid(appInfo.uid) else appInfo.label,
            appIcon = {
                AppIconImage(
                    packageInfo = appInfo.packageInfo,
                    label = appInfo.label,
                    modifier = Modifier
                        .padding(top = 4.dp)
                        .size(48.dp)
                )
            },
            appUid = uid,
            sharedUserId = if (isUidGroup) sharedUserId else "",
            appVersionName = if (isUidGroup) "" else (appInfo.packageInfo.versionName ?: ""),
            appVersionCode = if (isUidGroup) 0L else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                appInfo.packageInfo.longVersionCode
            } else {
                @Suppress("DEPRECATION")
                appInfo.packageInfo.versionCode.toLong()
            },
            profile = profile,
            isUidGroup = isUidGroup,
            affectedApps = sameUidApps,
            onViewTemplate = {
                getTemplateInfoById(it)?.let { info ->
                    navigator.push(Route.TemplateEditor(info, true))
                }
            },
            onManageTemplate = {
                navigator.push(Route.AppProfileTemplate)
            },
            onProfileChange = {
                scope.launch {
                    if (it.allowSu) {
                        // sync with allowlist.c - forbid_system_uid
                        if (uid < 2000 && uid != 1000) {
                            snackBarHost.showSnackbar(suNotAllowed)
                            return@launch
                        }
                        if (!it.rootUseDefault && it.rules.isNotEmpty() && !setSepolicy(profile.name, it.rules)) {
                            snackBarHost.showSnackbar(failToUpdateSepolicy)
                            return@launch
                        }
                    }
                    if (!Natives.setAppProfile(it)) {
                        snackBarHost.showSnackbar(failToUpdateAppProfile.format(appInfo.uid))
                    } else {
                        profile = it
                        viewModel.loadAppList()
                    }
                }
            },
        )
    }
}

@Composable
private fun AppProfileInner(
    modifier: Modifier = Modifier,
    packageName: String,
    appLabel: String,
    appIcon: @Composable (() -> Unit),
    appUid: Int,
    sharedUserId: String = "",
    appVersionName: String,
    appVersionCode: Long,
    profile: Natives.Profile,
    isUidGroup: Boolean = false,
    affectedApps: List<SuperUserViewModel.AppInfo> = emptyList(),
    onViewTemplate: (id: String) -> Unit = {},
    onManageTemplate: () -> Unit = {},
    onProfileChange: (Natives.Profile) -> Unit,
) {
    val isRootGranted = profile.allowSu
    val userId = appUid / 100000
    val appId = appUid % 100000

    val initialRootMode = if (profile.rootUseDefault) {
        Mode.Default
    } else if (profile.rootTemplate != null) {
        Mode.Template
    } else {
        Mode.Custom
    }
    var rootMode by rememberSaveable(profile) {
        mutableStateOf(initialRootMode)
    }
    val nonRootMode = if (profile.nonRootUseDefault) Mode.Default else Mode.Custom
    val mode = if (isRootGranted) rootMode else nonRootMode

    Column(modifier = modifier) {
        SegmentedColumn(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp),
            content = listOf(
                {
                    SegmentedListItem(
                        headlineContent = { Text(appLabel) },
                        supportingContent = {
                            Column {
                                if (!isUidGroup) {
                                    Text("$appVersionName ($appVersionCode)", color = MaterialTheme.colorScheme.outline)
                                    Text(packageName, color = MaterialTheme.colorScheme.outline)
                                } else {
                                    if (sharedUserId.isNotEmpty()) {
                                        Text(text = sharedUserId, color = MaterialTheme.colorScheme.outline)
                                    }
                                    Text(
                                        text = stringResource(R.string.group_contains_apps, affectedApps.size),
                                        color = MaterialTheme.colorScheme.outline
                                    )
                                }
                            }
                        },
                        leadingContent = appIcon,
                        trailingContent = {
                            Column {
                                if (userId != 0) {
                                    StatusTag(
                                        label = "USER $userId",
                                        contentColor = MaterialTheme.colorScheme.onTertiary,
                                        backgroundColor = MaterialTheme.colorScheme.tertiary
                                    )
                                    StatusTag(
                                        label = "UID $appId",
                                        contentColor = MaterialTheme.colorScheme.onTertiary,
                                        backgroundColor = MaterialTheme.colorScheme.tertiary
                                    )
                                } else {
                                    StatusTag(
                                        label = "UID $appUid",
                                        contentColor = MaterialTheme.colorScheme.onTertiary,
                                        backgroundColor = MaterialTheme.colorScheme.tertiary
                                    )
                                }
                            }
                        }
                    )
                },
                {
                    SegmentedSwitchItem(
                        icon = Icons.Filled.Security,
                        title = stringResource(id = R.string.superuser),
                        checked = isRootGranted,
                        onCheckedChange = { onProfileChange(profile.copy(allowSu = it)) },
                    )
                },
                {
                    SegmentedListItem(
                        headlineContent = { Text(stringResource(R.string.profile)) },
                        supportingContent = { Text(mode.text, color = MaterialTheme.colorScheme.outline) },
                        leadingContent = { Icon(Icons.Filled.AccountCircle, null) },
                    )
                }
            )
        )

        Crossfade(targetState = isRootGranted, label = "") { current ->
            Column(
                modifier = Modifier.padding(bottom = 6.dp + 48.dp + 6.dp /* SnackBar height */)
            ) {
                if (current) {
                    ProfileBox(mode, true) {
                        // template mode shouldn't change profile here!
                        if (it == Mode.Default || it == Mode.Custom) {
                            onProfileChange(
                                profile.copy(
                                    rootUseDefault = it == Mode.Default,
                                    rootTemplate = null
                                )
                            )
                        }
                        rootMode = it
                    }
                    AnimatedVisibility(
                        visible = mode == Mode.Template,
                        enter = expandVertically() + fadeIn(),
                        exit = shrinkVertically() + fadeOut()
                    ) {
                        TemplateConfig(
                            profile = profile,
                            onViewTemplate = onViewTemplate,
                            onManageTemplate = onManageTemplate,
                            onProfileChange = onProfileChange
                        )
                    }
                    AnimatedVisibility(
                        visible = mode == Mode.Custom,
                        enter = expandVertically() + fadeIn(),
                        exit = shrinkVertically() + fadeOut()
                    ) {
                        RootProfileConfig(
                            fixedName = true,
                            enabled = mode == Mode.Custom,
                            profile = profile,
                            onProfileChange = onProfileChange
                        )
                    }
                } else {
                    ProfileBox(mode, false) {
                        onProfileChange(profile.copy(nonRootUseDefault = (it == Mode.Default)))
                    }
                    AnimatedVisibility(
                        visible = true,
                        enter = expandVertically() + fadeIn(),
                        exit = shrinkVertically() + fadeOut()
                    ) {
                        AppProfileConfig(
                            fixedName = true,
                            profile = profile,
                            enabled = mode == Mode.Custom,
                            onProfileChange = onProfileChange
                        )
                    }
                }
                if (isUidGroup) {
                    val appItems = affectedApps.map<SuperUserViewModel.AppInfo, @Composable () -> Unit> { app ->
                        {
                            SegmentedListItem(
                                headlineContent = { Text(app.label) },
                                supportingContent = { Text(app.packageName) },
                                leadingContent = {
                                    AppIconImage(
                                        packageInfo = app.packageInfo,
                                        label = app.label,
                                        modifier = Modifier.size(36.dp)
                                    )
                                }
                            )
                        }
                    }
                    SegmentedColumn(
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                        title = stringResource(R.string.app_profile_affects_following_apps),
                        content = appItems
                    )
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun TopBar(
    onBack: () -> Unit,
    scrollBehavior: TopAppBarScrollBehavior? = null,
    isUidGroup: Boolean = false,
    packageName: String = "",
) {
    LargeFlexibleTopAppBar(
        title = { Text(stringResource(R.string.profile)) },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null) }
        },
        actions = {
            if (!isUidGroup) {
                var showDropdown by remember { mutableStateOf(false) }

                IconButton(
                    onClick = { showDropdown = true },
                ) {
                    Icon(
                        imageVector = Icons.Filled.MoreVert,
                        contentDescription = stringResource(id = R.string.settings)
                    )
                    DropdownMenu(
                        expanded = showDropdown,
                        onDismissRequest = { showDropdown = false }
                    ) {
                        DropdownMenuItem(
                            text = { Text(stringResource(id = R.string.launch_app)) },
                            onClick = {
                                showDropdown = false
                                launchApp(packageName)
                            },
                        )
                        DropdownMenuItem(
                            text = { Text(stringResource(id = R.string.force_stop_app)) },
                            onClick = {
                                showDropdown = false
                                forceStopApp(packageName)
                            },
                        )
                        DropdownMenuItem(
                            text = { Text(stringResource(id = R.string.restart_app)) },
                            onClick = {
                                showDropdown = false
                                restartApp(packageName)
                            },
                        )
                    }
                }
            }
        },
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface,
            scrolledContainerColor = MaterialTheme.colorScheme.surface
        ),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun ProfileBox(
    mode: Mode,
    hasTemplate: Boolean,
    onModeChange: (Mode) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(ButtonGroupDefaults.ConnectedSpaceBetween),
    ) {
        val options = listOf(
            Mode.Default to stringResource(R.string.profile_default),
            Mode.Template to stringResource(R.string.profile_template),
            Mode.Custom to stringResource(R.string.profile_custom),
        )

        options.forEachIndexed { index, (m, label) ->
            ToggleButton(
                checked = mode == m,
                onCheckedChange = {
                    if (m != Mode.Template || hasTemplate) onModeChange(m)
                },
                enabled = if (m == Mode.Template) hasTemplate else true,
                modifier = Modifier
                    .weight(1f)
                    .semantics { role = Role.RadioButton },
                shapes = when (index) {
                    0 -> ButtonGroupDefaults.connectedLeadingButtonShapes()
                    options.lastIndex -> ButtonGroupDefaults.connectedTrailingButtonShapes()
                    else -> ButtonGroupDefaults.connectedMiddleButtonShapes()
                },
            ) {
                Text(label, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
        }
    }
}

@Preview
@Composable
private fun AppProfilePreview() {
    var profile by remember { mutableStateOf(Natives.Profile("")) }
    AppProfileInner(
        packageName = "icu.nullptr.test",
        appLabel = "Test",
        appIcon = { Icon(Icons.Filled.Android, null) },
        appUid = 1,
        appVersionName = "v1.0.0",
        appVersionCode = 12345,
        profile = profile,
        onProfileChange = {
            profile = it
        },
    )
}
