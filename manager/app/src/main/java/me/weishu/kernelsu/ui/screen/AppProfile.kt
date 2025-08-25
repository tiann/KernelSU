package me.weishu.kernelsu.ui.screen

import android.widget.Toast
import androidx.compose.animation.Crossfade
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AccountCircle
import androidx.compose.material.icons.rounded.Security
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.dropUnlessResumed
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.destinations.AppProfileTemplateScreenDestination
import com.ramcosta.composedestinations.generated.destinations.TemplateEditorScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.DropdownItem
import me.weishu.kernelsu.ui.component.SuperDropdown
import me.weishu.kernelsu.ui.component.profile.AppProfileConfig
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.component.profile.TemplateConfig
import me.weishu.kernelsu.ui.util.forceStopApp
import me.weishu.kernelsu.ui.util.getSepolicy
import me.weishu.kernelsu.ui.util.launchApp
import me.weishu.kernelsu.ui.util.restartApp
import me.weishu.kernelsu.ui.util.setSepolicy
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import me.weishu.kernelsu.ui.viewmodel.getTemplateInfoById
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopup
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.extra.SuperSwitch
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.icon.icons.useful.ImmersionMore
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

/**
 * @author weishu
 * @date 2023/5/16.
 */
@Composable
@Destination<RootGraph>
fun AppProfileScreen(
    navigator: DestinationsNavigator,
    appInfo: SuperUserViewModel.AppInfo,
) {
    val context = LocalContext.current
    val scrollBehavior = MiuixScrollBehavior()
    val scope = rememberCoroutineScope()
    val failToUpdateAppProfile = stringResource(R.string.failed_to_update_app_profile).format(appInfo.label).format(appInfo.uid)
    val failToUpdateSepolicy = stringResource(R.string.failed_to_update_sepolicy).format(appInfo.label)
    val suNotAllowed = stringResource(R.string.su_not_allowed).format(appInfo.label)

    val packageName = appInfo.packageName
    val initialProfile = Natives.getAppProfile(packageName, appInfo.uid)
    if (initialProfile.allowSu) {
        initialProfile.rules = getSepolicy(packageName)
    }
    var profile by rememberSaveable {
        mutableStateOf(initialProfile)
    }

    Scaffold(
        topBar = {
            TopBar(
                onBack = dropUnlessResumed { navigator.popBackStack() },
                packageName = packageName,
                scrollBehavior = scrollBehavior,
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .height(getWindowSize().height.dp)
                .padding(top = 16.dp)
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection),
            contentPadding = innerPadding,
            overscrollEffect = null
        ) {
            item {
                AppProfileInner(
                    packageName = appInfo.packageName,
                    appLabel = appInfo.label,
                    appIcon = {
                        AppIconImage(
                            packageInfo = appInfo.packageInfo,
                            label = appInfo.label,
                            modifier = Modifier
                                .size(60.dp)
                        )
                    },
                    profile = profile,
                    onViewTemplate = {
                        getTemplateInfoById(it)?.let { info ->
                            navigator.navigate(TemplateEditorScreenDestination(info)) {
                                launchSingleTop = true
                            }
                        }
                    },
                    onManageTemplate = {
                        navigator.navigate(AppProfileTemplateScreenDestination()) {
                            launchSingleTop = true
                        }
                    },
                    onProfileChange = {
                        scope.launch {
                            if (it.allowSu) {
                                // sync with allowlist.c - forbid_system_uid
                                if (appInfo.uid < 2000 && appInfo.uid != 1000) {
                                    Toast.makeText(context, suNotAllowed, Toast.LENGTH_SHORT).show()
                                    return@launch
                                }
                                if (!it.rootUseDefault && it.rules.isNotEmpty() && !setSepolicy(profile.name, it.rules)) {
                                    Toast.makeText(context, failToUpdateSepolicy, Toast.LENGTH_SHORT).show()
                                    return@launch
                                }
                            }
                            if (!Natives.setAppProfile(it)) {
                                Toast.makeText(context, failToUpdateAppProfile, Toast.LENGTH_SHORT).show()
                            } else {
                                profile = it
                            }
                        }
                    },
                )
                Spacer(
                    Modifier.height(
                        WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                WindowInsets.captionBar.asPaddingValues().calculateBottomPadding()
                    )
                )
            }
        }
    }
}

@Composable
private fun AppProfileInner(
    modifier: Modifier = Modifier,
    packageName: String,
    appLabel: String,
    appIcon: @Composable () -> Unit,
    profile: Natives.Profile,
    onViewTemplate: (id: String) -> Unit = {},
    onManageTemplate: () -> Unit = {},
    onProfileChange: (Natives.Profile) -> Unit,
) {
    val isRootGranted = profile.allowSu

    Column(
        modifier = modifier
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp)
                .padding(bottom = 12.dp),
        ) {

            Row(
                modifier = Modifier.padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                appIcon()
                Column(
                    modifier = Modifier.padding(start = 16.dp),
                ) {
                    Text(
                        text = appLabel,
                        fontSize = 17.5.sp,
                        color = colorScheme.onSurface,
                        fontWeight = FontWeight(500)
                    )
                    Text(
                        text = packageName,
                        fontSize = 16.sp,
                        color = colorScheme.onSurfaceVariantSummary
                    )
                }
            }
        }


        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp)
                .padding(bottom = 12.dp),
        ) {
            SuperSwitch(
                leftAction = {
                    Icon(
                        imageVector = Icons.Rounded.Security,
                        contentDescription = null,
                        modifier = Modifier.padding(end = 16.dp),
                        tint = colorScheme.onBackground
                    )
                },
                title = stringResource(id = R.string.superuser),
                checked = isRootGranted,
                onCheckedChange = { onProfileChange(profile.copy(allowSu = it)) },
            )
        }

        Crossfade(targetState = isRootGranted, label = "") { current ->
            Column(
                modifier = Modifier.padding(bottom = 12.dp)
            ) {
                //SmallTitle(text = stringResource(R.string.profile))
                if (current) {
                    val initialMode = if (profile.rootUseDefault) {
                        Mode.Default
                    } else if (profile.rootTemplate != null) {
                        Mode.Template
                    } else {
                        Mode.Custom
                    }
                    var mode by rememberSaveable {
                        mutableStateOf(initialMode)
                    }
                    ProfileBox(mode, true) {
                        // template mode shouldn't change profile here!
                        if (it == Mode.Default || it == Mode.Custom) {
                            onProfileChange(profile.copy(rootUseDefault = it == Mode.Default))
                        }
                        mode = it
                    }

                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 12.dp)
                            .padding(bottom = 12.dp),
                    ) {
                        Crossfade(targetState = mode, label = "") { currentMode ->
                            if (currentMode == Mode.Default) {
                                Spacer(Modifier.height(0.dp))
                            }
                            if (currentMode == Mode.Template) {
                                TemplateConfig(
                                    profile = profile,
                                    onViewTemplate = onViewTemplate,
                                    onManageTemplate = onManageTemplate,
                                    onProfileChange = onProfileChange
                                )
                            } else if (mode == Mode.Custom) {
                                RootProfileConfig(
                                    fixedName = true,
                                    profile = profile,
                                    onProfileChange = onProfileChange
                                )
                            }
                        }
                    }
                } else {
                    val mode = if (profile.nonRootUseDefault) Mode.Default else Mode.Custom
                    ProfileBox(mode, false) {
                        onProfileChange(profile.copy(nonRootUseDefault = (it == Mode.Default)))
                    }
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 12.dp)
                            .padding(bottom = 12.dp),
                    ) {
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
}

private enum class Mode() {
    Default(),
    Template(),
    Custom();
}

@Composable
private fun TopBar(
    onBack: () -> Unit,
    packageName: String,
    scrollBehavior: ScrollBehavior,
) {
    TopAppBar(
        title = stringResource(R.string.profile),
        navigationIcon = {
            IconButton(
                modifier = Modifier.padding(start = 16.dp),
                onClick = onBack
            ) {
                Icon(
                    imageVector = MiuixIcons.Useful.Back,
                    contentDescription = null,
                    tint = colorScheme.onBackground
                )
            }
        },
        actions = {
            val showTopPopup = remember { mutableStateOf(false) }
            IconButton(
                modifier = Modifier.padding(end = 16.dp),
                onClick = { showTopPopup.value = true },
                holdDownState = showTopPopup.value
            ) {
                Icon(
                    imageVector = MiuixIcons.Useful.ImmersionMore,
                    tint = colorScheme.onSurface,
                    contentDescription = stringResource(id = R.string.settings)
                )
            }
            ListPopup(
                show = showTopPopup,
                onDismissRequest = { showTopPopup.value = false },
                popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                alignment = PopupPositionProvider.Align.TopRight,
            ) {
                ListPopupColumn {
                    val items = listOf(
                        stringResource(id = R.string.launch_app),
                        stringResource(id = R.string.force_stop_app),
                        stringResource(id = R.string.restart_app)
                    )

                    items.forEachIndexed { index, text ->
                        DropdownItem(
                            text = text,
                            optionSize = items.size,
                            index = index,
                            onSelectedIndexChange = { selectedIndex ->
                                when (selectedIndex) {
                                    0 -> launchApp(packageName)
                                    1 -> forceStopApp(packageName)
                                    2 -> restartApp(packageName)
                                }
                                showTopPopup.value = false
                            }
                        )
                    }
                }
            }
        },
        scrollBehavior = scrollBehavior
    )
}

@Composable
private fun ProfileBox(
    mode: Mode,
    hasTemplate: Boolean,
    onModeChange: (Mode) -> Unit,
) {
    val defaultText = stringResource(R.string.profile_default)
    val templateText = stringResource(R.string.profile_template)
    val customText = stringResource(R.string.profile_custom)
    val list =
        remember(hasTemplate, defaultText, templateText, customText) {
            buildList {
                add(defaultText)
                if (hasTemplate) {
                    add(templateText)
                }
                add(customText)
            }
        }

    val modesAndTitles = remember(hasTemplate, defaultText, templateText, customText) {
        buildList {
            add(Mode.Default to defaultText)
            if (hasTemplate) {
                add(Mode.Template to templateText)
            }
            add(Mode.Custom to customText)
        }
    }
    val selectedIndex = modesAndTitles.indexOfFirst { it.first == mode }
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp)
            .padding(bottom = 12.dp),
    ) {
        SuperDropdown(
            title = stringResource(R.string.profile),
            items = list,
            leftAction = {
                Icon(
                    Icons.Rounded.AccountCircle,
                    modifier = Modifier.padding(end = 16.dp),
                    contentDescription = null,
                    tint = colorScheme.onBackground
                )
            },
            selectedIndex = if (selectedIndex == -1) 0 else selectedIndex,
        ) {
            onModeChange(modesAndTitles[it].first)
        }
    }
}
