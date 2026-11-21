package me.weishu.kernelsu.ui.mochi

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.core.content.FileProvider
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.component.uninstalldialog.UninstallDialog
import me.weishu.kernelsu.ui.screen.settings.SettingsScreenActions
import me.weishu.kernelsu.ui.screen.settings.SettingsUiState
import me.weishu.kernelsu.ui.util.getBugreportFile
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

@Composable
fun SettingPagerMochi(uiState: SettingsUiState, actions: SettingsScreenActions) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val loading = rememberLoadingDialog()
    var showUninstall by rememberSaveable { mutableStateOf(false) }
    val ksuValid = Natives.isManager
    val logSaved = stringResource(R.string.log_saved)
    val sendLogText = stringResource(R.string.send_log)
    val exportLauncher = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/gzip")) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            loading.withLoading {
                withContext(Dispatchers.IO) {
                    context.contentResolver.openOutputStream(uri)?.use { out ->
                        getBugreportFile(context).inputStream().use { it.copyTo(out) }
                    }
                }
            }
            Toast.makeText(context, logSaved, Toast.LENGTH_SHORT).show()
        }
    }

    @Composable
    fun featureSummary(status: String, normal: Int): String = stringResource(
        when (status) {
            "unsupported" -> R.string.feature_status_unsupported_summary
            "managed" -> R.string.feature_status_managed_summary
            "forced" -> R.string.feature_status_forced_summary
            else -> normal
        },
    )

    MochiLazyLargeTitleScaffold(title = stringResource(R.string.settings)) {
        item(key = "update") {
            MochiListSection {
                KsuSwitchRow(
                    stringResource(R.string.settings_check_update),
                    uiState.checkUpdate,
                    actions.onSetCheckUpdate,
                    icon = "cloud-download",
                    color = KsuTileColors.Blue,
                    subtitle = stringResource(R.string.settings_check_update_summary),
                )
                if (ksuValid) {
                    KsuSwitchRow(
                        stringResource(R.string.settings_module_check_update),
                        uiState.checkModuleUpdate,
                        actions.onSetCheckModuleUpdate,
                        icon = "sync",
                        color = KsuTileColors.Blue,
                        subtitle = stringResource(R.string.settings_check_update_summary),
                    )
                }
            }
        }
        item(key = "ui") {
            MochiListSection {
                MochiListPickerRow(
                    stringResource(R.string.settings_ui_mode),
                    options = UiMode.entries.map { it.name }.toImmutableList(),
                    selectedIndex = UiMode.entries.indexOf(UiMode.fromValue(uiState.uiMode)),
                    onSelect = actions.onSetUiModeIndex,
                    leading = { KsuTile("browsers", KsuTileColors.Indigo) },
                    titleTrailing = if (MochiNews.unseen(context)) {
                        { NewUiDot() }
                    } else {
                        null
                    },
                )
                MochiListRow(
                    stringResource(R.string.settings_theme),
                    subtitle = stringResource(R.string.settings_theme_summary),
                    leading = { KsuTile("color-palette", KsuTileColors.Indigo) },
                    chevron = true,
                    onClick = actions.onOpenTheme,
                )
            }
        }
        if (ksuValid) {
            item(key = "template") {
                MochiListSection {
                    MochiListRow(
                        stringResource(R.string.settings_profile_template),
                        subtitle = stringResource(R.string.settings_profile_template_summary),
                        leading = { KsuTile("document-text", KsuTileColors.Orange) },
                        chevron = true,
                        onClick = actions.onOpenProfileTemplate,
                    )
                }
            }
            item(key = "features") {
                MochiListSection {
                    MochiListPickerRow(
                        stringResource(R.string.settings_sucompat),
                        options = persistentListOf(
                            stringResource(R.string.settings_mode_enable_by_default),
                            stringResource(R.string.settings_mode_disable_until_reboot),
                            stringResource(R.string.settings_mode_disable_always),
                        ),
                        selectedIndex = uiState.suCompatMode,
                        onSelect = actions.onSetSuCompatMode,
                        enabled = uiState.suCompatStatus == "supported",
                        leading = { KsuTile("shield", KsuTileColors.Green, uiState.suCompatStatus == "supported") },
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_kernel_umount),
                        uiState.isKernelUmountEnabled,
                        actions.onSetKernelUmountEnabled,
                        icon = "layers",
                        color = KsuTileColors.Green,
                        subtitle = featureSummary(uiState.kernelUmountStatus, R.string.settings_kernel_umount_summary),
                        enabled = uiState.kernelUmountStatus == "supported",
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_selinux_hide),
                        uiState.isSelinuxHideEnabled,
                        actions.onSetSelinuxHideEnabled,
                        icon = "eye-off",
                        color = KsuTileColors.Green,
                        subtitle = featureSummary(uiState.selinuxHideStatus, R.string.settings_selinux_hide_summary),
                        enabled = uiState.selinuxHideStatus == "supported",
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_sulog),
                        uiState.isSulogEnabled,
                        actions.onSetSulogEnabled,
                        icon = "reader",
                        color = KsuTileColors.Green,
                        subtitle = featureSummary(uiState.sulogStatus, R.string.settings_sulog_summary),
                        enabled = uiState.sulogStatus == "supported",
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_adb_root),
                        uiState.isAdbRootEnabled,
                        actions.onSetAdbRootEnabled,
                        icon = "terminal",
                        color = KsuTileColors.Green,
                        subtitle = featureSummary(uiState.adbRootStatus, R.string.settings_adb_root_summary),
                        enabled = uiState.adbRootStatus == "supported",
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_shell_su_adb_only),
                        uiState.isShellSuAdbOnlyEnabled,
                        actions.onSetShellSuAdbOnlyEnabled,
                        icon = "shield-checkmark",
                        color = KsuTileColors.Green,
                        subtitle = featureSummary(uiState.shellSuAdbOnlyStatus, R.string.settings_shell_su_adb_only_summary),
                        enabled = uiState.shellSuAdbOnlyStatus == "supported",
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_soft_reboot),
                        uiState.isLateLoadMode || uiState.useSoftReboot,
                        actions.onSetUseSoftReboot,
                        icon = "reload",
                        color = KsuTileColors.Green,
                        subtitle = stringResource(R.string.settings_soft_reboot_summary),
                        enabled = !uiState.isLateLoadMode,
                    )
                }
            }
            item(key = "advanced") {
                MochiListSection {
                    KsuSwitchRow(
                        stringResource(R.string.settings_umount_modules_default),
                        uiState.isDefaultUmountModules,
                        actions.onSetDefaultUmountModules,
                        icon = "git-compare",
                        subtitle = stringResource(R.string.settings_umount_modules_default_summary),
                    )
                    KsuSwitchRow(
                        stringResource(R.string.enable_web_debugging),
                        uiState.enableWebDebugging,
                        actions.onSetEnableWebDebugging,
                        icon = "code-slash",
                        subtitle = stringResource(R.string.enable_web_debugging_summary),
                    )
                    KsuSwitchRow(
                        stringResource(R.string.settings_auto_jailbreak),
                        uiState.autoJailbreak,
                        actions.onSetAutoJailbreak,
                        icon = "flash",
                        subtitle = stringResource(R.string.settings_auto_jailbreak_summary),
                        enabled = uiState.isLateLoadMode,
                    )
                }
            }
        }
        if (uiState.isLkmMode) {
            item(key = "uninstall") {
                MochiListSection {
                    MochiListRow(
                        stringResource(R.string.settings_uninstall),
                        leading = { KsuTile("trash", KsuTileColors.Red, !uiState.isLateLoadMode) },
                        enabled = !uiState.isLateLoadMode,
                        destructive = true,
                        onClick = { showUninstall = true },
                    )
                }
            }
        }
        item(key = "about") {
            MochiListSection {
                MochiListRow(
                    stringResource(R.string.save_log),
                    leading = { KsuTile("save") },
                    onClick = {
                        val now = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd_HH_mm"))
                        exportLauncher.launch("KernelSU_bugreport_$now.tar.gz")
                    },
                )
                MochiListRow(
                    sendLogText,
                    leading = { KsuTile("share") },
                    onClick = {
                        scope.launch {
                            val bugreport = loading.withLoading { withContext(Dispatchers.IO) { getBugreportFile(context) } }
                            val uri = FileProvider.getUriForFile(context, "${BuildConfig.APPLICATION_ID}.fileprovider", bugreport)
                            val share = Intent(Intent.ACTION_SEND).apply {
                                putExtra(Intent.EXTRA_STREAM, uri)
                                setDataAndType(uri, "application/gzip")
                                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                            }
                            context.startActivity(Intent.createChooser(share, sendLogText))
                        }
                    },
                )
                MochiListRow(
                    stringResource(R.string.about),
                    leading = { KsuTile("information-circle", KsuTileColors.Blue) },
                    chevron = true,
                    onClick = actions.onOpenAbout,
                )
            }
        }
    }
    UninstallDialog(show = showUninstall, onDismissRequest = { showUninstall = false })
}
