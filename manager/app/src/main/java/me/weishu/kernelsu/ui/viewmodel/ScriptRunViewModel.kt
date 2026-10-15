package me.weishu.kernelsu.ui.viewmodel

import android.util.Log
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.registry.DownloadVerificationException
import me.weishu.kernelsu.data.script.ReleasedException
import me.weishu.kernelsu.data.script.ScriptCommand
import me.weishu.kernelsu.data.script.ScriptPackageException
import me.weishu.kernelsu.data.script.ScriptRepository
import me.weishu.kernelsu.data.script.UnverifiedScriptException
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.screen.scriptrun.ScriptRunArg
import me.weishu.kernelsu.ui.screen.scriptrun.ScriptRunPhase
import me.weishu.kernelsu.ui.screen.scriptrun.ScriptRunUiState
import me.weishu.kernelsu.ui.util.removeAsRoot
import me.weishu.kernelsu.ui.util.runOneshotScript
import java.util.UUID

/** Prepares a one-shot script, collects its parameters and runs it once. */
class ScriptRunViewModel : ViewModel() {

    private val _state = MutableStateFlow(ScriptRunUiState())
    val state: StateFlow<ScriptRunUiState> = _state.asStateFlow()

    /** Full stdout + stderr, for "save log". */
    val logContent = StringBuilder()

    private var visit: String? = null
    private var prepareJob: Job? = null

    /** Token of the current preparation, owning its extraction in [ScriptRepository]. */
    @Volatile
    private var owner: String? = null

    /**
     * Prepares [arg] once per page visit ([visit] survives configuration changes, so a
     * finished run stays on screen after a rotation but a new visit starts over).
     */
    fun load(arg: ScriptRunArg, visit: String) {
        if (this.visit == visit || _state.value.phase == ScriptRunPhase.RUNNING) return
        this.visit = visit
        synchronized(logContent) { logContent.setLength(0) }
        _state.value = ScriptRunUiState(displayName = arg.displayName, offerInstall = arg.offerInstall)
        releaseExtraction()
        val token = UUID.randomUUID().toString()
        owner = token
        prepareJob = viewModelScope.launch {
            runCatching { ScriptRepository.prepare(arg.uri, arg.registryId, token) }
                .onSuccess { prepared ->
                    // Superseded or left: releaseExtraction() already released this token.
                    if (owner != token) return@onSuccess
                    val values = prepared.params.defaults
                    val lastRun = withContext(Dispatchers.IO) { ScriptRepository.history.last(prepared.manifest.id) }
                    _state.update {
                        it.copy(
                            phase = ScriptRunPhase.READY,
                            prepared = prepared,
                            values = values,
                            errors = prepared.params.validate(values),
                            lastRun = lastRun,
                        )
                    }
                }
                .onFailure { e ->
                    if (e is CancellationException) throw e
                    if (e is ReleasedException || owner != token) return@onFailure
                    Log.w(TAG, "cannot prepare script ${arg.uri}", e)
                    _state.update { it.copy(phase = ScriptRunPhase.FAILED, error = errorMessage(e)) }
                }
        }
    }

    fun setValue(key: String, value: String) {
        _state.update { s ->
            val prepared = s.prepared ?: return@update s
            if (s.phase != ScriptRunPhase.READY) return@update s
            val values = s.values + (key to value)
            s.copy(values = values, errors = prepared.params.validate(values))
        }
    }

    fun run() {
        val current = _state.value
        val prepared = current.prepared ?: return
        if (!current.canRun) return
        val dir = prepared.dir.absolutePath
        val token = owner ?: return
        val command = try {
            ScriptCommand.build(
                dir,
                ScriptCommand.environment(ScriptRepository.hostInfo(), prepared.manifest.id, dir, prepared.params, current.values),
            )
        } catch (e: IllegalArgumentException) {
            Log.w(TAG, "invalid parameters", e)
            return
        }
        _state.update { it.copy(phase = ScriptRunPhase.RUNNING, output = "") }
        viewModelScope.launch {
            var text = ""
            val exitCode = withContext(Dispatchers.IO) {
                val code = runOneshotScript(
                    command = command,
                    onStdout = { line ->
                        val tempText = "$line\n"
                        text = if (tempText.startsWith("\u001b[H\u001b[J")) { // clear command
                            tempText.substring(6)
                        } else {
                            text + tempText
                        }
                        _state.update { it.copy(output = text) }
                        synchronized(logContent) { logContent.append(line).append('\n') }
                    },
                    onStderr = { line ->
                        synchronized(logContent) { logContent.append(line).append('\n') }
                    },
                )
                ScriptRepository.recordRun(prepared, code)
                // Nothing stays behind: the script may have created root-owned files in its directory.
                runCatching { removeAsRoot(dir) }
                    .onSuccess { removed -> if (!removed) Log.w(TAG, "cannot remove $dir") }
                    .onFailure { Log.w(TAG, "cannot remove $dir", it) }
                ScriptRepository.release(token)
                if (owner == token) owner = null
                code
            }
            _state.update {
                it.copy(
                    phase = ScriptRunPhase.FINISHED,
                    exitCode = exitCode,
                    lastRun = ScriptRepository.history.last(prepared.manifest.id),
                )
            }
        }
    }

    /**
     * The page is gone (not just recreated): drop the extraction and start over on the next
     * visit. A running script cleans up when it ends, even after the page is gone.
     */
    fun leave() {
        if (_state.value.phase == ScriptRunPhase.RUNNING) return
        releaseExtraction()
        visit = null
        _state.value = ScriptRunUiState()
    }

    private fun releaseExtraction() {
        prepareJob?.cancel()
        prepareJob = null
        owner?.let(ScriptRepository::release)
        owner = null
    }

    override fun onCleared() {
        if (_state.value.phase != ScriptRunPhase.RUNNING) releaseExtraction()
    }

    private fun errorMessage(e: Throwable): String {
        val res = ksuApp.resources
        return when (e) {
            is UnverifiedScriptException -> res.getString(R.string.script_error_unverified)
            is DownloadVerificationException -> res.getString(R.string.script_error_verify_failed)
            is ScriptPackageException -> when (e.reason) {
                ScriptPackageException.Reason.CORRUPT -> res.getString(R.string.script_error_corrupt)
                ScriptPackageException.Reason.UNSAFE_ENTRY -> res.getString(R.string.script_error_unsafe)
                ScriptPackageException.Reason.TOO_LARGE -> res.getString(R.string.script_error_too_large)
                ScriptPackageException.Reason.MISSING_MODULE_PROP -> res.getString(R.string.script_error_missing_prop)
                ScriptPackageException.Reason.NOT_ONESHOT -> res.getString(R.string.script_error_not_oneshot)
                ScriptPackageException.Reason.INVALID_ID -> res.getString(R.string.script_error_invalid_id)
                ScriptPackageException.Reason.ID_MISMATCH -> res.getString(R.string.script_error_id_mismatch)
                ScriptPackageException.Reason.MISSING_ACTION -> res.getString(R.string.script_error_missing_action)
                ScriptPackageException.Reason.INVALID_PARAMS ->
                    res.getString(R.string.script_error_params, e.cause?.message ?: e.message.orEmpty())
            }

            else -> res.getString(R.string.script_error_generic, e.message ?: e.javaClass.simpleName)
        }
    }

    private companion object {
        const val TAG = "ScriptRunViewModel"
    }
}
