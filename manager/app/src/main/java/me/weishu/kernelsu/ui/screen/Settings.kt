package me.weishu.kernelsu.ui.screen

import android.content.Context
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Adb
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.ContactPage
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.DeleteForever
import androidx.compose.material.icons.rounded.DeveloperMode
import androidx.compose.material.icons.rounded.EnhancedEncryption
import androidx.compose.material.icons.rounded.Fence
import androidx.compose.material.icons.rounded.FolderDelete
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.RemoveCircle
import androidx.compose.material.icons.rounded.RemoveModerator
import androidx.compose.material.icons.rounded.RestartAlt
import androidx.compose.material.icons.rounded.Update
import androidx.compose.material.icons.rounded.UploadFile
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.core.content.edit
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.destinations.AboutScreenDestination
import com.ramcosta.composedestinations.generated.destinations.AppProfileTemplateScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.SendLogDialog
import me.weishu.kernelsu.ui.component.UninstallDialog
import me.weishu.kernelsu.ui.component.rememberLoadingDialog
import me.weishu.kernelsu.ui.util.execKsud
import me.weishu.kernelsu.ui.util.getFeaturePersistValue
import me.weishu.kernelsu.ui.util.getFeatureStatus
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperDropdown
import top.yukonga.miuix.kmp.extra.SuperSwitch
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

/**
 * @author weishu
 * @date 2023/1/1.
 */
@Composable
@Destination<RootGraph>
fun SettingPager(
    navigator: DestinationsNavigator,
    bottomInnerPadding: Dp
) {
    val scrollBehavior = MiuixScrollBehavior()
    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = colorScheme.surface,
        tint = HazeTint(colorScheme.surface.copy(0.8f))
    )

    Scaffold(
        topBar = {
            TopAppBar(
                modifier = Modifier.hazeEffect(hazeState) {
                    style = hazeStyle
                    blurRadius = 30.dp
                    noiseFactor = 0f
                },
                color = Color.Transparent,
                title = stringResource(R.string.settings),
                scrollBehavior = scrollBehavior
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val loadingDialog = rememberLoadingDialog()

        val showUninstallDialog = rememberSaveable { mutableStateOf(false) }
        val uninstallDialog = UninstallDialog(showUninstallDialog, navigator)
        val showSendLogDialog = rememberSaveable { mutableStateOf(false) }
        val sendLogDialog = SendLogDialog(showSendLogDialog, loadingDialog)

        LazyColumn(
            modifier = Modifier
                .fillMaxHeight()
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .hazeSource(state = hazeState)
                .padding(horizontal = 12.dp),
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                val context = LocalContext.current
                val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
                var checkUpdate by rememberSaveable {
                    mutableStateOf(prefs.getBoolean("check_update", true))
                }

                Card(
                    modifier = Modifier
                        .padding(top = 12.dp)
                        .fillMaxWidth(),
                ) {
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
                        onCheckedChange = {
                            prefs.edit {
                                putBoolean("check_update", it)
                            }
                            checkUpdate = it
                        }
                    )
                    KsuIsValid {
                        var checkModuleUpdate by rememberSaveable {
                            mutableStateOf(prefs.getBoolean("module_check_update", true))
                        }
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_module_check_update),
                            summary = stringResource(id = R.string.settings_check_update_summary),
                            leftAction = {
                                Icon(
                                    Icons.Rounded.UploadFile,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.settings_check_update),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = checkModuleUpdate,
                            onCheckedChange = {
                                prefs.edit {
                                    putBoolean("module_check_update", it)
                                }
                                checkModuleUpdate = it
                            }
                        )
                    }
                }

                Card(
                    modifier = Modifier
                        .padding(top = 12.dp)
                        .fillMaxWidth(),
                ) {
                    val themeItems = listOf(
                        stringResource(id = R.string.settings_theme_mode_system),
                        stringResource(id = R.string.settings_theme_mode_light),
                        stringResource(id = R.string.settings_theme_mode_dark),
                        stringResource(id = R.string.settings_theme_mode_monet_system),
                        stringResource(id = R.string.settings_theme_mode_monet_light),
                        stringResource(id = R.string.settings_theme_mode_monet_dark),
                    )
                    var themeMode by rememberSaveable {
                        mutableIntStateOf(prefs.getInt("color_mode", 0))
                    }
                    SuperDropdown(
                        title = stringResource(id = R.string.settings_theme),
                        summary = stringResource(id = R.string.settings_theme_summary),
                        items = themeItems,
                        leftAction = {
                            Icon(
                                Icons.Rounded.Palette,
                                modifier = Modifier.padding(end = 16.dp),
                                contentDescription = stringResource(id = R.string.settings_theme),
                                tint = colorScheme.onBackground
                            )
                        },
                        selectedIndex = themeMode,
                        onSelectedIndexChange = { index ->
                            prefs.edit { putInt("color_mode", index) }
                            themeMode = index
                        }
                    )

                    AnimatedVisibility(
                        visible = themeMode in 3..5
                    ) {
                        val colorItems = listOf(
                            stringResource(id = R.string.settings_key_color_default),
                            stringResource(id = R.string.color_blue),
                            stringResource(id = R.string.color_red),
                            stringResource(id = R.string.color_green),
                            stringResource(id = R.string.color_purple),
                            stringResource(id = R.string.color_orange),
                            stringResource(id = R.string.color_teal),
                            stringResource(id = R.string.color_pink),
                            stringResource(id = R.string.color_brown),
                        )
                        val colorValues = listOf(
                            0,
                            Color(0xFF1A73E8).toArgb(),
                            Color(0xFFEA4335).toArgb(),
                            Color(0xFF34A853).toArgb(),
                            Color(0xFF9333EA).toArgb(),
                            Color(0xFFFB8C00).toArgb(),
                            Color(0xFF009688).toArgb(),
                            Color(0xFFE91E63).toArgb(),
                            Color(0xFF795548).toArgb(),
                        )
                        var keyColorIndex by rememberSaveable {
                            mutableIntStateOf(
                                colorValues.indexOf(prefs.getInt("key_color", 0)).takeIf { it >= 0 } ?: 0
                            )
                        }
                        SuperDropdown(
                            title = stringResource(id = R.string.settings_key_color),
                            summary = stringResource(id = R.string.settings_key_color_summary),
                            items = colorItems,
                            leftAction = {
                                Icon(
                                    Icons.Rounded.Palette,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.settings_key_color),
                                    tint = colorScheme.onBackground
                                )
                            },
                            selectedIndex = keyColorIndex,
                            onSelectedIndexChange = { index ->
                                prefs.edit { putInt("key_color", colorValues[index]) }
                                keyColorIndex = index
                            }
                        )
                    }
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

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        val modeItems = listOf(
                            stringResource(id = R.string.settings_mode_default),
                            stringResource(id = R.string.settings_mode_temp_enable),
                            stringResource(id = R.string.settings_mode_always_enable),
                        )
                        val currentEnhancedEnabled = Natives.isEnhancedSecurityEnabled()
                        var enhancedSecurityMode by rememberSaveable { mutableIntStateOf(if (currentEnhancedEnabled) 1 else 0) }
                        val enhancedPersistValue by produceState(initialValue = null as Long?) {
                            value = getFeaturePersistValue("enhanced_security")
                        }
                        println("Enhanced persist value: $enhancedPersistValue")
                        LaunchedEffect(enhancedPersistValue) {
                            enhancedPersistValue?.let { v ->
                                enhancedSecurityMode = if (v != 0L) 2 else if (currentEnhancedEnabled) 1 else 0
                            }
                        }
                        val enhancedStatus by produceState(initialValue = "") {
                            value = getFeatureStatus("enhanced_security")
                        }
                        val enhancedSummary = when (enhancedStatus) {
                            "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                            "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                            else -> stringResource(id = R.string.settings_enable_enhanced_security_summary)
                        }
                        SuperDropdown(
                            title = stringResource(id = R.string.settings_enable_enhanced_security),
                            summary = enhancedSummary,
                            items = modeItems,
                            leftAction = {
                                Icon(
                                    Icons.Rounded.EnhancedEncryption,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.settings_enable_enhanced_security),
                                    tint = colorScheme.onBackground
                                )
                            },
                            enabled = enhancedStatus == "supported",
                            selectedIndex = enhancedSecurityMode,
                            onSelectedIndexChange = { index ->
                                when (index) {
                                    // Default: disable and save to persist
                                    0 -> if (Natives.setEnhancedSecurityEnabled(false)) {
                                        execKsud("feature save", true)
                                        prefs.edit { putInt("enhanced_security_mode", 0) }
                                        enhancedSecurityMode = 0
                                    }

                                    // Temporarily enable: save disabled state first, then enable
                                    1 -> if (Natives.setEnhancedSecurityEnabled(false)) {
                                        execKsud("feature save", true)
                                        if (Natives.setEnhancedSecurityEnabled(true)) {
                                            prefs.edit { putInt("enhanced_security_mode", 0) }
                                            enhancedSecurityMode = 1
                                        }
                                    }

                                    // Permanently enable: enable and save
                                    2 -> if (Natives.setEnhancedSecurityEnabled(true)) {
                                        execKsud("feature save", true)
                                        prefs.edit { putInt("enhanced_security_mode", 2) }
                                        enhancedSecurityMode = 2
                                    }
                                }
                            }
                        )

                        val currentSuEnabled = Natives.isSuEnabled()
                        var suCompatMode by rememberSaveable { mutableIntStateOf(if (!currentSuEnabled) 1 else 0) }
                        val suPersistValue by produceState(initialValue = null as Long?) {
                            value = getFeaturePersistValue("su_compat")
                        }
                        LaunchedEffect(suPersistValue) {
                            suPersistValue?.let { v ->
                                suCompatMode = if (v == 0L) 2 else if (!currentSuEnabled) 1 else 0
                            }
                        }
                        val suStatus by produceState(initialValue = "") {
                            value = getFeatureStatus("su_compat")
                        }
                        val suSummary = when (suStatus) {
                            "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                            "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                            else -> stringResource(id = R.string.settings_disable_su_summary)
                        }
                        SuperDropdown(
                            title = stringResource(id = R.string.settings_disable_su),
                            summary = suSummary,
                            items = modeItems,
                            leftAction = {
                                Icon(
                                    Icons.Rounded.RemoveModerator,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.settings_disable_su),
                                    tint = colorScheme.onBackground
                                )
                            },
                            enabled = suStatus == "supported",
                            selectedIndex = suCompatMode,
                            onSelectedIndexChange = { index ->
                                when (index) {
                                    // Default: enable and save to persist
                                    0 -> if (Natives.setSuEnabled(true)) {
                                        execKsud("feature save", true)
                                        prefs.edit { putInt("su_compat_mode", 0) }
                                        suCompatMode = 0
                                    }

                                    // Temporarily disable: save enabled state first, then disable
                                    1 -> if (Natives.setSuEnabled(true)) {
                                        execKsud("feature save", true)
                                        if (Natives.setSuEnabled(false)) {
                                            prefs.edit { putInt("su_compat_mode", 0) }
                                            suCompatMode = 1
                                        }
                                    }

                                    // Permanently disable: disable and save
                                    2 -> if (Natives.setSuEnabled(false)) {
                                        execKsud("feature save", true)
                                        prefs.edit { putInt("su_compat_mode", 2) }
                                        suCompatMode = 2
                                    }
                                }
                            }
                        )

                        val currentUmountEnabled = Natives.isKernelUmountEnabled()
                        var kernelUmountMode by rememberSaveable { mutableIntStateOf(if (!currentUmountEnabled) 1 else 0) }
                        val umountPersistValue by produceState(initialValue = null as Long?) {
                            value = getFeaturePersistValue("kernel_umount")
                        }
                        LaunchedEffect(umountPersistValue) {
                            umountPersistValue?.let { v ->
                                kernelUmountMode = if (v == 0L) 2 else if (!currentUmountEnabled) 1 else 0
                            }
                        }
                        val umountStatus by produceState(initialValue = "") {
                            value = getFeatureStatus("kernel_umount")
                        }
                        val umountSummary = when (umountStatus) {
                            "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                            "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                            else -> stringResource(id = R.string.settings_disable_kernel_umount_summary)
                        }
                        SuperDropdown(
                            title = stringResource(id = R.string.settings_disable_kernel_umount),
                            summary = umountSummary,
                            items = modeItems,
                            leftAction = {
                                Icon(
                                    Icons.Rounded.RemoveCircle,
                                    modifier = Modifier.padding(end = 16.dp),
                                    contentDescription = stringResource(id = R.string.settings_disable_kernel_umount),
                                    tint = colorScheme.onBackground
                                )
                            },
                            enabled = umountStatus == "supported",
                            selectedIndex = kernelUmountMode,
                            onSelectedIndexChange = { index ->
                                when (index) {
                                    // Default: enable and save to persist
                                    0 -> if (Natives.setKernelUmountEnabled(true)) {
                                        execKsud("feature save", true)
                                        prefs.edit { putInt("kernel_umount_mode", 0) }
                                        kernelUmountMode = 0
                                    }

                                    // Temporarily disable: save enabled state first, then disable
                                    1 -> if (Natives.setKernelUmountEnabled(true)) {
                                        execKsud("feature save", true)
                                        if (Natives.setKernelUmountEnabled(false)) {
                                            prefs.edit { putInt("kernel_umount_mode", 0) }
                                            kernelUmountMode = 1
                                        }
                                    }

                                    // Permanently disable: disable and save
                                    2 -> if (Natives.setKernelUmountEnabled(false)) {
                                        execKsud("feature save", true)
                                        prefs.edit { putInt("kernel_umount_mode", 2) }
                                        kernelUmountMode = 2
                                    }
                                }
                            }
                        )
                    }

                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        var umountChecked by rememberSaveable { mutableStateOf(Natives.isDefaultUmountModules()) }
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
                            onCheckedChange = {
                                if (Natives.setDefaultUmountModules(it)) {
                                    umountChecked = it
                                }
                            }
                        )

                        var enableWebDebugging by rememberSaveable {
                            mutableStateOf(prefs.getBoolean("enable_web_debugging", false))
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
                            onCheckedChange = {
                                prefs.edit { putBoolean("enable_web_debugging", it) }
                                enableWebDebugging = it
                            }
                        )
                    }
                }

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        val lkmMode = Natives.isLkmMode
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
                Spacer(Modifier.height(bottomInnerPadding))
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
