package me.weishu.kernelsu.ui.viewmodel

import me.weishu.kernelsu.data.model.RepoModule
import me.weishu.kernelsu.ui.component.SearchStatus

data class ModuleRepoUiState(
    val isRefreshing: Boolean = false,
    val modules: List<RepoModule> = emptyList(),
    val searchStatus: SearchStatus = SearchStatus(""),
    val searchResults: List<RepoModule> = emptyList(),
    val error: Throwable? = null
)
