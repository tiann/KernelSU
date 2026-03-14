package me.weishu.kernelsu.ui.viewmodel

import android.content.Context
import android.util.Log
import android.widget.Toast
import androidx.core.content.edit
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.repository.ModuleRepoRepository
import me.weishu.kernelsu.data.repository.ModuleRepoRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.screen.modulerepo.ModuleRepoUiState
import me.weishu.kernelsu.ui.util.isNetworkAvailable

class ModuleRepoViewModel(
    private val repo: ModuleRepoRepository = ModuleRepoRepositoryImpl()
) : ViewModel() {

    companion object {
        private const val TAG = "ModuleRepoViewModel"
    }

    typealias RepoModule = me.weishu.kernelsu.data.model.RepoModule

    private val _uiState = MutableStateFlow(ModuleRepoUiState())
    val uiState: StateFlow<ModuleRepoUiState> = _uiState.asStateFlow()

    private val prefs = ksuApp.getSharedPreferences("settings", Context.MODE_PRIVATE)
    private val searchQuery = MutableStateFlow("")

    init {
        _uiState.update {
            it.copy(
                sortByName = prefs.getBoolean("module_repo_sort_name", false),
                offline = !isNetworkAvailable(ksuApp)
            )
        }

        viewModelScope.launchSearchQueryCollector(searchQuery, ::applySearchText)
    }

    private fun filterModules(modules: List<RepoModule>, text: String): List<RepoModule> {
        if (text.isEmpty()) return emptyList()

        return modules.filter {
            it.moduleId.contains(text, true) ||
                    it.moduleName.contains(text, true) ||
                    it.authors.contains(text, true) ||
                    it.summary.contains(text, true) ||
                    me.weishu.kernelsu.ui.util.HanziToPinyin.getInstance().toPinyinString(it.moduleName)
                        .contains(text, true)
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

        val result = withContext(Dispatchers.IO) {
            filterModules(_uiState.value.modules, text)
        }

        _uiState.update {
            it.copy(
                searchResults = result,
                searchStatus = it.searchStatus.copy(resultStatus = searchResultStatusFor(text, result.isEmpty()))
            )
        }
    }

    private fun refreshSearchResults() {
        val state = _uiState.value
        val text = state.searchStatus.searchText
        val results = filterModules(state.modules, text)
        _uiState.update {
            it.copy(
                searchResults = results,
                searchStatus = it.searchStatus.copy(resultStatus = searchResultStatusFor(text, results.isEmpty()))
            )
        }
    }

    fun refresh() {
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
                result.onSuccess { modules ->
                    _uiState.update {
                        it.copy(
                            modules = modules,
                            isRefreshing = false,
                            offline = !isNetworkAvailable(ksuApp)
                        )
                    }
                    refreshSearchResults()
                }.onFailure { e ->
                    Log.e(TAG, "fetch modules failed", e)
                    Toast.makeText(
                        ksuApp,
                        ksuApp.getString(R.string.network_offline), Toast.LENGTH_SHORT
                    ).show()
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

    fun toggleSortByName() {
        val newValue = !_uiState.value.sortByName
        prefs.edit { putBoolean("module_repo_sort_name", newValue) }
        _uiState.update { it.copy(sortByName = newValue) }
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
