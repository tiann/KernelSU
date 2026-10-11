package me.weishu.kernelsu.ui.viewmodel

import android.util.Log
import android.widget.Toast
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.registry.UnsupportedSchemaException
import me.weishu.kernelsu.data.repository.ModuleRepoRepository
import me.weishu.kernelsu.data.repository.ModuleRepoRepositoryImpl
import me.weishu.kernelsu.data.repository.SettingsRepository
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.screen.modulerepo.ModuleRepoUiState
import me.weishu.kernelsu.ui.screen.modulerepo.RepoSort
import me.weishu.kernelsu.ui.util.PinyinUtil
import me.weishu.kernelsu.ui.util.isNetworkAvailable

class ModuleRepoViewModel(
    private val repo: ModuleRepoRepository = ModuleRepoRepositoryImpl(),
    private val settingsRepo: SettingsRepository = SettingsRepositoryImpl()
) : ViewModel() {

    companion object {
        private const val TAG = "ModuleRepoViewModel"
    }

    typealias RepoModule = me.weishu.kernelsu.data.model.RepoModule

    private val _uiState = MutableStateFlow(ModuleRepoUiState())
    val uiState: StateFlow<ModuleRepoUiState> = _uiState.asStateFlow()

    private val searchQuery = MutableStateFlow("")

    // Serializes the sorted writes of modules / searchResults, so a slower sort never replaces
    // a newer list or order with one computed from an older snapshot.
    private val listMutex = Mutex()

    init {
        val initial = RepoSort.fromStored(settingsRepo.moduleRepoSortOrder, null)
        _uiState.update {
            it.copy(
                sortOrder = initial,
                offline = !isNetworkAvailable(ksuApp)
            )
        }

        viewModelScope.launchSearchQueryCollector(searchQuery, ::applySearchText)
    }

    private fun sortModules(list: List<RepoModule>, order: RepoSort): List<RepoModule> = order.sort(list)

    private fun filterModules(modules: List<RepoModule>, text: String): List<RepoModule> {
        if (text.isEmpty()) return emptyList()

        return modules.filter {
            it.moduleId.contains(text, true) ||
                    it.moduleName.contains(text, true) ||
                    it.authors.contains(text, true) ||
                    it.summary.contains(text, true) ||
                    it.tags.any { tag -> tag.contains(text, true) } ||
                    PinyinUtil.toPinyin(it.moduleName).contains(text, true)
        }
    }

    private suspend fun applySearchText(text: String) {
        _uiState.update {
            it.copy(
                searchStatus = it.searchStatus.copy(
                    resultStatus = searchLoadingStatusFor(text)
                )
            )
        }

        if (text.isEmpty()) {
            _uiState.update { state ->
                state.copy(
                    searchResults = emptyList(),
                    searchStatus = state.searchStatus.copy(resultStatus = SearchStatus.ResultStatus.DEFAULT)
                )
            }
            return
        }

        listMutex.withLock {
            val result = withContext(Dispatchers.IO) {
                sortModules(filterModules(_uiState.value.modules, text), _uiState.value.sortOrder)
            }

            _uiState.update {
                it.copy(
                    searchResults = result,
                    searchStatus = it.searchStatus.copy(resultStatus = searchResultStatusFor(text, result.isEmpty()))
                )
            }
        }
    }

    private fun refreshSearchResults() {
        val state = _uiState.value
        val text = state.searchStatus.searchText
        val results = sortModules(filterModules(state.modules, text), state.sortOrder)
        _uiState.update {
            it.copy(
                searchResults = results,
                searchStatus = it.searchStatus.copy(resultStatus = searchResultStatusFor(text, results.isEmpty()))
            )
        }
    }

    fun refresh() {
        if (_uiState.value.isRefreshing) return
        viewModelScope.launch {
            _uiState.update {
                it.copy(
                    isRefreshing = true,
                    error = null,
                    offline = !isNetworkAvailable(ksuApp)
                )
            }
            val result = repo.fetchModules()

            withContext(Dispatchers.Main) {
                result.onSuccess { list ->
                    if (list.error != null) {
                        Toast.makeText(ksuApp, errorMessage(list.error, cached = true), Toast.LENGTH_SHORT).show()
                    }
                    listMutex.withLock {
                        val order = _uiState.value.sortOrder
                        val sorted = withContext(Dispatchers.Default) { sortModules(list.modules, order) }
                        _uiState.update {
                            it.copy(
                                hasLoaded = true,
                                modules = sorted,
                                stale = list.stale,
                                error = list.error,
                                offline = !isNetworkAvailable(ksuApp)
                            )
                        }
                        refreshSearchResults()
                    }
                    _uiState.update { it.copy(isRefreshing = false) }
                }.onFailure { e ->
                    Log.e(TAG, "fetch modules failed", e)
                    Toast.makeText(ksuApp, errorMessage(e, cached = false), Toast.LENGTH_SHORT).show()
                    _uiState.update {
                        it.copy(
                            isRefreshing = false,
                            error = e,
                            offline = !isNetworkAvailable(ksuApp)
                        )
                    }
                }
            }
        }
    }

    private fun errorMessage(e: Throwable, cached: Boolean): String = when {
        e is UnsupportedSchemaException -> ksuApp.getString(R.string.module_repo_unsupported_schema)
        cached -> ksuApp.getString(R.string.module_repo_using_cache)
        !isNetworkAvailable(ksuApp) -> ksuApp.getString(R.string.network_offline)
        else -> ksuApp.getString(R.string.module_repo_load_failed)
    }

    fun setSortOrder(order: RepoSort) {
        settingsRepo.moduleRepoSortOrder = order.name
        viewModelScope.launch {
            listMutex.withLock {
                // Checked under the lock: an earlier change may still be pending.
                val state = _uiState.value
                if (state.sortOrder == order) return@withLock
                val (sortedModules, sortedSearch) = withContext(Dispatchers.Default) {
                    sortModules(state.modules, order) to sortModules(state.searchResults, order)
                }
                _uiState.update {
                    it.copy(
                        sortOrder = order,
                        modules = sortedModules,
                        searchResults = sortedSearch,
                    )
                }
            }
        }
    }

    fun updateSearchStatus(status: SearchStatus) {
        val previous = _uiState.value.searchStatus
        _uiState.update { it.copy(searchStatus = status) }
        if (previous.searchText != status.searchText) {
            searchQuery.value = status.searchText
        }
    }

    fun updateSearchText(text: String) {
        updateSearchStatus(_uiState.value.searchStatus.copy(searchText = text))
    }
}
