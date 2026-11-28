package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.toImmutableList
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.screen.install.InstallMethod
import me.weishu.kernelsu.ui.screen.install.InstallScreenActions
import me.weishu.kernelsu.ui.screen.install.InstallUiState
import me.weishu.kernelsu.ui.util.LkmSelection
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.MochiIconButton
import ui.feice.mochi.basic.Text
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListDisclosure
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold

@Composable
internal fun InstallScreenMochi(state: InstallUiState, actions: InstallScreenActions) {
    val inactiveConfirm = rememberConfirmDialog(onConfirm = { actions.onSelectMethod(InstallMethod.DirectInstallToInactiveSlot) })
    val alertTitle = stringResource(android.R.string.dialog_alert_title)
    val inactiveWarning = stringResource(R.string.install_inactive_slot_warning)
    val onMethod: (InstallMethod) -> Unit = { option ->
        when (option) {
            is InstallMethod.SelectFile -> actions.onSelectBootImage()
            is InstallMethod.DownloadFile -> actions.onDownloadFile()
            is InstallMethod.DirectInstall -> actions.onSelectMethod(option)
            is InstallMethod.DirectInstallToInactiveSlot -> inactiveConfirm.showConfirm(alertTitle, inactiveWarning)
        }
    }
    val methodIcons = mapOf(
        InstallMethod.SelectFile::class.java to "document",
        InstallMethod.DownloadFile::class.java to "cloud-download",
        InstallMethod.DirectInstall::class.java to "flash",
        InstallMethod.DirectInstallToInactiveSlot::class.java to "swap-horizontal",
    )
    val clearLabel = stringResource(android.R.string.cancel)

    Box(Modifier.fillMaxSize()) {
        MochiLazyCompactTitleScaffold(title = stringResource(R.string.install), navigationIcon = { MochiBackButton() }) {
            item(key = "method") {
                MochiListSection {
                    state.installMethodOptions.forEach { option ->
                        MochiListRow(
                            stringResource(option.label),
                            subtitle = option.summary,
                            leading = { KsuTile(methodIcons[option.javaClass] ?: "flash", KsuTileColors.Blue) },
                            checked = option.javaClass == state.installMethod?.javaClass,
                            onClick = { onMethod(option) },
                        )
                    }
                }
            }
            if (state.canSelectPartition) {
                item(key = "partition") {
                    val download = state.installMethod is InstallMethod.DownloadFile
                    val items = if (download) state.remoteDisplayPartitions else state.displayPartitions
                    val index = if (download) state.remotePartitionSelectionIndex else state.partitionSelectionIndex
                    MochiListSection {
                        MochiListPickerRow(
                            if (download) {
                                stringResource(R.string.install_select_partition)
                            } else {
                                "${stringResource(R.string.install_select_partition)} (${state.slotSuffix})"
                            },
                            options = items.toImmutableList(),
                            selectedIndex = index.coerceIn(0, (items.size - 1).coerceAtLeast(0)),
                            onSelect = actions.onSelectPartition,
                            enabled = items.isNotEmpty(),
                            leading = { KsuTile("albums", KsuTileColors.Indigo) },
                        )
                    }
                }
            }
            if (state.canForceBackup) {
                item(key = "backup") {
                    MochiListSection {
                        KsuSwitchRow(
                            stringResource(R.string.install_force_backup),
                            state.forceBackup,
                            actions.onSelectForceBackup,
                            icon = "archive",
                            subtitle = stringResource(R.string.install_force_backup_summary),
                        )
                    }
                }
            }
            item(key = "lkm") {
                val lkm = state.lkmSelection as? LkmSelection.LkmUri
                MochiListSection {
                    MochiListRow(
                        stringResource(R.string.install_upload_lkm_file),
                        subtitle = lkm?.let { stringResource(R.string.selected_lkm, it.uri.lastPathSegment ?: "(file)") },
                        leading = { KsuTile("cube", KsuTileColors.Orange) },
                        trailing = lkm?.let {
                            {
                                MochiIconButton(onClick = actions.onClearLkm, modifier = Modifier.semantics { contentDescription = clearLabel }) {
                                    MochiIonicon("close-circle")
                                }
                            }
                        },
                        chevron = lkm == null,
                        onClick = actions.onUploadLkm,
                    )
                }
            }
            item(key = "advanced") {
                MochiListCard(animateHeight = false) {
                    MochiListDisclosure(
                        stringResource(R.string.advanced_options),
                        expanded = state.advancedOptionsShown,
                        onExpandedChange = { actions.onAdvancedOptionsClicked() },
                    ) {
                        KsuSwitchRow(
                            stringResource(R.string.allow_shell),
                            state.allowShell,
                            actions.onSelectAllowShell,
                            subtitle = stringResource(R.string.allow_shell_summary),
                        )
                        KsuSwitchRow(
                            stringResource(R.string.enable_adb),
                            state.enableAdb,
                            actions.onSelectEnableAdb,
                            subtitle = stringResource(R.string.enable_adb_summary),
                        )
                    }
                }
            }
            item(key = "space") { Box(Modifier.padding(bottom = 72.dp)) }
        }
        Box(Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 16.dp)) {
            MochiButton(onClick = actions.onNext, enabled = state.installMethod != null, style = MochiButtonStyle.ProminentGlass) {
                Text(stringResource(R.string.install_next))
                MochiIonicon("arrow-forward", size = 18.dp, modifier = Modifier.padding(start = 6.dp))
            }
        }
    }
}
