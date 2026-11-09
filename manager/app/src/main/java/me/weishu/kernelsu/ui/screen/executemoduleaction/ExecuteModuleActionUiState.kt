package me.weishu.kernelsu.ui.screen.executemoduleaction

import androidx.compose.runtime.Immutable

@Immutable
data class ExecuteModuleActionUiState(
    val text: String,
    val isComplete: Boolean = false,
    /** Top bar title; defaults to "Action". */
    val title: String? = null,
    /** Outcome shown below the output once complete (one-shot scripts). */
    val resultNotice: ActionResultNotice? = null,
)

enum class ActionResultKind { SUCCESS, NOT_APPLICABLE, FAILURE }

@Immutable
data class ActionResultNotice(val kind: ActionResultKind, val text: String)

@Immutable
data class ExecuteModuleActionScreenActions(
    val onBack: () -> Unit,
    val onSaveLog: () -> Unit,
    val onClose: () -> Unit = {},
)
