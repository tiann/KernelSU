package me.weishu.kernelsu.ui.screen.scriptrun

import android.net.Uri
import android.os.Parcelable
import androidx.compose.runtime.Immutable
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.data.script.ParamError
import me.weishu.kernelsu.data.script.PreparedScript
import me.weishu.kernelsu.data.script.ScriptRunRecord

/** Navigation argument of the script run page. */
@Parcelize
data class ScriptRunArg(
    val uri: Uri,
    /** Registry flow: the ZIP must be a verified registry download with this module.prop id. */
    val registryId: String? = null,
    /** Local ZIP from "install from storage": also offer installing it as a module. */
    val offerInstall: Boolean = false,
    /** Shown in the title area until module.prop has been read. */
    val displayName: String? = null,
) : Parcelable

enum class ScriptRunPhase {
    PREPARING,
    FAILED,
    READY,
    RUNNING,
    FINISHED,
}

@Immutable
data class ScriptRunUiState(
    val phase: ScriptRunPhase = ScriptRunPhase.PREPARING,
    val displayName: String? = null,
    val offerInstall: Boolean = false,
    val prepared: PreparedScript? = null,
    /** Localized reason the script cannot be run (phase FAILED). */
    val error: String? = null,
    /** Current input per param key. */
    val values: Map<String, String> = emptyMap(),
    val errors: Map<String, ParamError> = emptyMap(),
    val lastRun: ScriptRunRecord? = null,
    val output: String = "",
    val exitCode: Int? = null,
) {
    val canRun: Boolean get() = phase == ScriptRunPhase.READY && errors.isEmpty()
}

@Immutable
data class ScriptRunActions(
    val onBack: () -> Unit,
    val onValueChange: (key: String, value: String) -> Unit,
    val onRun: () -> Unit,
    val onInstallAsModule: () -> Unit,
)
