package me.weishu.kernelsu.ui.screen.executemoduleaction

import androidx.compose.runtime.Immutable

@Immutable
data class ExecuteModuleActionUiState(
    val text: String,
)

@Immutable
data class ExecuteModuleActionScreenActions(
    val onBack: () -> Unit,
    val onSaveLog: () -> Unit,
)
