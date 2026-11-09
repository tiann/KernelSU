package me.weishu.kernelsu.ui.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import me.weishu.kernelsu.data.model.RepoModuleDetail
import me.weishu.kernelsu.data.registry.ModuleNotInIndexException
import me.weishu.kernelsu.data.repository.ModuleRepoRepository
import me.weishu.kernelsu.data.repository.ModuleRepoRepositoryImpl

/** Loads the registry detail (and README) of one module for the detail screen. */
class ModuleRepoDetailViewModel(
    private val repo: ModuleRepoRepository = ModuleRepoRepositoryImpl(),
) : ViewModel() {

    data class State(
        val moduleId: String? = null,
        val detail: RepoModuleDetail? = null,
        val loaded: Boolean = false,
        /** Not in the current index (e.g. suspended): never offer installation. */
        val notInIndex: Boolean = false,
        val loadFailed: Boolean = false,
    )

    private val _state = MutableStateFlow(State())
    val state: StateFlow<State> = _state.asStateFlow()

    private var job: Job? = null

    /** Loads [moduleId] unless it is already loaded; [force] reloads (retry). */
    fun load(moduleId: String, force: Boolean = false) {
        val current = _state.value
        if (!force && current.moduleId == moduleId && (current.loaded || job?.isActive == true)) return
        if (current.moduleId != moduleId) _state.value = State(moduleId = moduleId)
        job?.cancel()
        job = viewModelScope.launch {
            _state.update { it.copy(loaded = false) }
            repo.fetchDetail(moduleId)
                .onSuccess { detail ->
                    _state.update { it.copy(detail = detail, notInIndex = false, loadFailed = false, loaded = true) }
                }
                .onFailure { e ->
                    _state.update {
                        if (e is ModuleNotInIndexException) {
                            it.copy(detail = null, notInIndex = true, loaded = true)
                        } else {
                            it.copy(loadFailed = it.detail == null, loaded = true)
                        }
                    }
                }
        }
    }
}
