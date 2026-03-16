package me.weishu.kernelsu.ui.screen.install

import androidx.compose.runtime.Immutable
import me.weishu.kernelsu.ui.util.LkmSelection

@Immutable
internal data class InstallUiState(
    val installMethod: InstallMethod?,
    val lkmSelection: LkmSelection,
    val partitionSelectionIndex: Int,
    val displayPartitions: List<String>,
    val currentKmi: String,
    val slotSuffix: String,
    val installMethodOptions: List<InstallMethod>,
    val canSelectPartition: Boolean,
    val advancedOptionsShown: Boolean,
    val allowShell: Boolean,
    val enableAdb: Boolean,
)

@Immutable
internal data class InstallScreenActions(
    val onBack: () -> Unit,
    val onSelectMethod: (InstallMethod) -> Unit,
    val onSelectBootImage: () -> Unit,
    val onUploadLkm: () -> Unit,
    val onClearLkm: () -> Unit,
    val onSelectPartition: (Int) -> Unit,
    val onNext: () -> Unit,
    val onAdvancedOptionsClicked: () -> Unit,
    val onSelectAllowShell: (Boolean) -> Unit,
    val onSelectEnableAdb: (Boolean) -> Unit,
)
