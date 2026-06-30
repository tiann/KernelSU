package me.weishu.kernelsu.ui.screen.appprofile

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
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AccountCircle
import androidx.compose.material.icons.filled.Android
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.ButtonGroupDefaults
import androidx.compose.material3.DropdownMenuGroup
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.DropdownMenuPopup
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuDefaults
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.material.ExpressiveScaffold
import me.weishu.kernelsu.ui.component.material.ExpressiveToggleButton
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.SegmentedSwitchItem
import me.weishu.kernelsu.ui.component.material.SnackBarHost
import me.weishu.kernelsu.ui.component.material.TopBarBackButton
import me.weishu.kernelsu.ui.component.material.expressiveTopAppBarColors
import me.weishu.kernelsu.ui.component.profile.AppProfileConfig
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.component.profile.TemplateConfig
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel

/**
 * @author weishu
 * @date 2023/5/16.
 */
@Composable
fun AppProfileScreenMaterial(
    state: AppProfileUiState,
    actions: AppProfileActions,
    snackBarHost: SnackbarHostState,
) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())

    ExpressiveScaffold(
        topBar = {
            TopBar(
                onBack = actions.onBack,
                scrollBehavior = scrollBehavior,
                isUidGroup = state.isUidGroup,
                packageName = state.packageName,
                userId = state.uid / 100000,
                onLaunchApp = actions.onLaunchApp,
                onForceStopApp = actions.onForceStopApp,
                onRestartApp = actions.onRestartApp,
            )
        },
        snackbarHost = { SnackBarHost(hostState = snackBarHost, modifier = Modifier.safeDrawingPadding()) },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { paddingValues ->
        AppProfileInner(
            modifier = Modifier
                .padding(paddingValues)
                .fillMaxHeight()
                .imePadding()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState()),
            packageName = if (state.isUidGroup) "" else state.appGroup.primary.packageName,
            appLabel = if (state.isUidGroup) ownerNameForUid(state.appGroup.primary.uid) else state.appGroup.primary.label,
            appIcon = {
                AppIconImage(
                    packageInfo = state.appGroup.primary.packageInfo,
                    label = state.appGroup.primary.label,
                    modifier = Modifier
                        .padding(top = 4.dp)
                        .size(48.dp)
                )
            },
            appUid = state.uid,
            sharedUserId = if (state.isUidGroup) state.sharedUserId else "",
            appVersionName = if (state.isUidGroup) "" else (state.appGroup.primary.packageInfo.versionName ?: ""),
            appVersionCode = if (state.isUidGroup) 0L else state.appGroup.primary.packageInfo.longVersionCode,
            profile = state.profile,
            isUidGroup = state.isUidGroup,
            affectedApps = state.appGroup.apps,
            onViewTemplate = actions.onViewTemplate,
            onManageTemplate = actions.onManageTemplate,
            onProfileChange = actions.onProfileChange,
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
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp),
            content = listOf(
                {
                    SegmentedListItem(
                        headlineContent = { Text(appLabel) },
                        supportingContent = {
                            Column {
                                if (!isUidGroup) {
                                    Text("$appVersionName ($appVersionCode)", color = MaterialTheme.colorScheme.onSurfaceVariant)
                                    Text(packageName, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                } else {
                                    if (sharedUserId.isNotEmpty()) {
                                        Text(text = sharedUserId, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                    }
                                    Text(
                                        text = stringResource(R.string.group_contains_apps, affectedApps.size),
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )
                                }
                            }
                        },
                        leadingContent = appIcon,
                        trailingContent = {
                            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                if (userId != 0) {
                                    StatusTag(
                                        label = "USER $userId",
                                        contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                                        backgroundColor = MaterialTheme.colorScheme.tertiaryContainer
                                    )
                                    StatusTag(
                                        label = "UID $appId",
                                        contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                                        backgroundColor = MaterialTheme.colorScheme.tertiaryContainer
                                    )
                                } else {
                                    StatusTag(
                                        label = "UID $appUid",
                                        contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                                        backgroundColor = MaterialTheme.colorScheme.tertiaryContainer
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
                        supportingContent = { Text(mode.text, color = MaterialTheme.colorScheme.onSurfaceVariant) },
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

@Composable
private fun TopBar(
    onBack: () -> Unit,
    scrollBehavior: TopAppBarScrollBehavior? = null,
    isUidGroup: Boolean = false,
    packageName: String = "",
    userId: Int = 0,
    onLaunchApp: (String, Int) -> Unit,
    onForceStopApp: (String, Int) -> Unit,
    onRestartApp: (String, Int) -> Unit,
) {
    val haptic = LocalHapticFeedback.current
    LargeFlexibleTopAppBar(
        title = { Text(stringResource(R.string.profile)) },
        navigationIcon = {
            TopBarBackButton(onClick = onBack)
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
                    DropdownMenuPopup(
                        expanded = showDropdown,
                        onDismissRequest = { showDropdown = false }
                    ) {
                        val menuItems = listOf(
                            R.string.launch_app to onLaunchApp,
                            R.string.force_stop_app to onForceStopApp,
                            R.string.restart_app to onRestartApp,
                        )
                        DropdownMenuGroup(shapes = MenuDefaults.groupShapes()) {
                            menuItems.forEachIndexed { index, (resId, action) ->
                                DropdownMenuItem(
                                    selected = false,
                                    onClick = {
                                        haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                        showDropdown = false
                                        action(packageName, userId)
                                    },
                                    text = { Text(stringResource(id = resId)) },
                                    shapes = MenuDefaults.itemShape(index = index, count = menuItems.size),
                                )
                            }
                        }
                    }
                }
            }
        },
        colors = expressiveTopAppBarColors(),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}

@Composable
private fun ProfileBox(
    mode: Mode,
    hasTemplate: Boolean,
    onModeChange: (Mode) -> Unit,
) {
    val haptic = LocalHapticFeedback.current
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(ButtonGroupDefaults.ConnectedSpaceBetween),
    ) {
        val options = listOf(
            Mode.Default to stringResource(R.string.profile_default),
            Mode.Template to stringResource(R.string.profile_template),
            Mode.Custom to stringResource(R.string.profile_custom),
        )

        options.forEachIndexed { index, (m, label) ->
            ExpressiveToggleButton(
                checked = mode == m,
                onCheckedChange = { checked ->
                    if (checked && (m != Mode.Template || hasTemplate)) {
                        haptic.performHapticFeedback(HapticFeedbackType.ContextClick)
                        onModeChange(m)
                    }
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
