package me.weishu.kernelsu.ui.screen

import android.content.Context
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
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Adb
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.Compress
import androidx.compose.material.icons.rounded.ContactPage
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.DeleteForever
import androidx.compose.material.icons.rounded.DeveloperMode
import androidx.compose.material.icons.rounded.Fence
import androidx.compose.material.icons.rounded.FolderDelete
import androidx.compose.material.icons.rounded.RemoveModerator
import androidx.compose.material.icons.rounded.RestartAlt
import androidx.compose.material.icons.rounded.Update
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.core.content.edit
import androidx.lifecycle.compose.dropUnlessResumed
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.destinations.AboutScreenDestination
import com.ramcosta.composedestinations.generated.destinations.AppProfileTemplateScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.ConfirmResult
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.SendLogDialog
import me.weishu.kernelsu.ui.component.UninstallDialog
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rememberLoadingDialog
import me.weishu.kernelsu.ui.util.shrinkModules
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperSwitch
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

/**
 * @author weishu
 * @date 2023/1/1.
 */
@Composable
@Destination<RootGraph>
fun SettingScreen(navigator: DestinationsNavigator) {
    val scrollBehavior = MiuixScrollBehavior()

    Scaffold(
        topBar = {
            TopBar(
                onBack = dropUnlessResumed { navigator.popBackStack() },
                scrollBehavior = scrollBehavior,
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val loadingDialog = rememberLoadingDialog()
        val shrinkDialog = rememberConfirmDialog()

        val showUninstallDialog = rememberSaveable { mutableStateOf(false) }
        val uninstallDialog = UninstallDialog(showUninstallDialog, navigator)
        val showSendLogDialog = rememberSaveable { mutableStateOf(false) }
        val sendLogDialog = SendLogDialog(showSendLogDialog, loadingDialog)

        LazyColumn(
            modifier = Modifier
                .height(getWindowSize().height.dp)
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .padding(horizontal = 12.dp),
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                val context = LocalContext.current
                val scope = rememberCoroutineScope()
                val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)

                Card(
                    modifier = Modifier
                        .padding(top = 12.dp)
                        .fillMaxWidth(),
                ) {
                    var checkUpdate by rememberSaveable {
                        mutableStateOf(
                            prefs.getBoolean("check_update", true)
                        )
                    }
                    SuperSwitch(
                        title = stringResource(id = R.string.settings_check_update),
                        summary = stringResource(id = R.string.settings_check_update_summary),
                        leftAction = {
                            Icon(
                                Icons.Rounded.Update,
                                modifier = Modifier.padding(end = 16.dp),
                                contentDescription = stringResource(id = R.string.settings_check_update),
                                tint = colorScheme.onBackground
                            )
                        },
                        checked = checkUpdate,
                        onCheckedChange = { it ->
                            prefs.edit { putBoolean("check_update", it) }
                            checkUpdate = it
                        }
                    )
                }

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        val profileTemplate = stringResource(id = R.string.settings_profile_template)
                        SuperArrow(
                            title = profileTemplate,
                            summary = stringResource(id = R.string.settings_profile_template_summary),
                            leftAction = {
                                Icon(
                                    Icons.Rounded.Fence,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = profileTemplate,
                                    tint = colorScheme.onBackground
                                )
                            },
                            onClick = {
                                navigator.navigate(AppProfileTemplateScreenDestination) {
                                    launchSingleTop = true
                                }
                            }
                        )
                    }
                }

                var umountChecked by rememberSaveable { mutableStateOf(Natives.isDefaultUmountModules()) }

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_umount_modules_default),
                            summary = stringResource(id = R.string.settings_umount_modules_default_summary),
                            leftAction = {
                                Icon(
                                    Icons.Rounded.FolderDelete,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.settings_umount_modules_default),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = umountChecked,
                            onCheckedChange = { it ->
                                if (Natives.setDefaultUmountModules(it)) {
                                    umountChecked = it
                                }
                            }
                        )
                        if (Natives.version >= Natives.MINIMAL_SUPPORTED_SU_COMPAT) {
                            var isSuDisabled by rememberSaveable {
                                mutableStateOf(!Natives.isSuEnabled())
                            }
                            SuperSwitch(
                                title = stringResource(id = R.string.settings_disable_su),
                                summary = stringResource(id = R.string.settings_disable_su_summary),
                                leftAction = {
                                    Icon(
                                        Icons.Rounded.RemoveModerator,
                                        modifier = Modifier.padding(end = 16.dp),
                                        contentDescription = stringResource(id = R.string.settings_disable_su),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = isSuDisabled,
                                onCheckedChange = { checked: Boolean ->
                                    val shouldEnable = !checked
                                    if (Natives.setSuEnabled(shouldEnable)) {
                                        isSuDisabled = !shouldEnable
                                    }
                                }
                            )
                        }


                        var enableWebDebugging by rememberSaveable {
                            mutableStateOf(
                                prefs.getBoolean("enable_web_debugging", false)
                            )
                        }

                        SuperSwitch(
                            title = stringResource(id = R.string.enable_web_debugging),
                            summary = stringResource(id = R.string.enable_web_debugging_summary),
                            leftAction = {
                                Icon(
                                    Icons.Rounded.DeveloperMode,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.enable_web_debugging),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = enableWebDebugging,
                            onCheckedChange = { it ->
                                prefs.edit { putBoolean("enable_web_debugging", it) }
                                enableWebDebugging = it
                            }
                        )
                    }
                }

                val shrink = stringResource(id = R.string.shrink_sparse_image)
                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        SuperArrow(
                            title = shrink,
                            leftAction = {
                                Icon(
                                    Icons.Rounded.Compress,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = shrink,
                                    tint = colorScheme.onBackground
                                )
                            },
                            onClick = {
                                scope.launch {
                                    val result = shrinkDialog.awaitConfirm(title = shrink)
                                    if (result == ConfirmResult.Confirmed) {
                                        loadingDialog.withLoading {
                                            shrinkModules()
                                        }
                                    }
                                }
                            },
                        )

                        val lkmMode = Natives.version >= Natives.MINIMAL_SUPPORTED_KERNEL_LKM && Natives.isLkmMode
                        if (lkmMode) {
                            val uninstall = stringResource(id = R.string.settings_uninstall)
                            SuperArrow(
                                title = uninstall,
                                leftAction = {
                                    Icon(
                                        Icons.Rounded.Delete,
                                        modifier = Modifier.padding(end = 16.dp),
                                        contentDescription = uninstall,
                                        tint = colorScheme.onBackground,
                                    )
                                },
                                onClick = {
                                    showUninstallDialog.value = true
                                    uninstallDialog
                                }
                            )
                        }
                    }
                }

                Card(
                    modifier = Modifier
                        .padding(vertical = 12.dp)
                        .fillMaxWidth(),
                ) {
                    SuperArrow(
                        title = stringResource(id = R.string.send_log),
                        leftAction = {
                            Icon(
                                Icons.Rounded.BugReport,
                                modifier = Modifier.padding(end = 16.dp),
                                contentDescription = stringResource(id = R.string.send_log),
                                tint = colorScheme.onBackground
                            )
                        },
                        onClick = {
                            showSendLogDialog.value = true
                            sendLogDialog
                        },
                    )
                    val about = stringResource(id = R.string.about)
                    SuperArrow(
                        title = about,
                        leftAction = {
                            Icon(
                                Icons.Rounded.ContactPage,
                                modifier = Modifier.padding(end = 16.dp),
                                contentDescription = about,
                                tint = colorScheme.onBackground
                            )
                        },
                        onClick = {
                            navigator.navigate(AboutScreenDestination) {
                                launchSingleTop = true
                            }
                        }
                    )
                }
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

enum class UninstallType(val icon: ImageVector, val title: Int, val message: Int) {
    TEMPORARY(
        Icons.Rounded.RemoveModerator,
        R.string.settings_uninstall_temporary,
        R.string.settings_uninstall_temporary_message
    ),
    PERMANENT(
        Icons.Rounded.DeleteForever,
        R.string.settings_uninstall_permanent,
        R.string.settings_uninstall_permanent_message
    ),
    RESTORE_STOCK_IMAGE(
        Icons.Rounded.RestartAlt,
        R.string.settings_restore_stock_image,
        R.string.settings_restore_stock_image_message
    ),
    NONE(Icons.Rounded.Adb, 0, 0)
}

@Composable
private fun TopBar(
    onBack: () -> Unit = {},
    scrollBehavior: ScrollBehavior,
) {
    TopAppBar(
        title = stringResource(R.string.settings),
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
        scrollBehavior = scrollBehavior
    )
}
