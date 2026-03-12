package me.weishu.kernelsu.ui.viewmodel

import android.os.SystemClock
import android.util.Log
import androidx.core.content.edit
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.data.model.ModuleUpdateInfo
import me.weishu.kernelsu.data.repository.ModuleRepository
import me.weishu.kernelsu.data.repository.ModuleRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.screen.module.ModuleUiState
import me.weishu.kernelsu.ui.util.hasMagisk
import java.text.Collator
import java.util.Locale

class ModuleViewModel(
    private val repo: ModuleRepository = ModuleRepositoryImpl()
) : ViewModel() {

    companion object {
        private const val TAG = "ModuleViewModel"
    }

    private data class ModuleUpdateSignature(
        val updateJson: String,
        val versionCode: Int,
        val enabled: Boolean,
        val update: Boolean,
        val remove: Boolean
    )

    private data class ModuleUpdateCache(
        val signature: ModuleUpdateSignature,
        val info: ModuleUpdateInfo
    )

    private val _uiState = MutableStateFlow(ModuleUiState())
    val uiState: StateFlow<ModuleUiState> = _uiState.asStateFlow()

    private val updateInfoMutex = Mutex()
    private var updateInfoCache: MutableMap<String, ModuleUpdateCache> = mutableMapOf()
    private val updateInfoInFlight = mutableSetOf<String>()
    private val searchQuery = MutableStateFlow("")

    var isNeedRefresh = false
        private set

    init {
        viewModelScope.launchSearchQueryCollector(searchQuery, ::applySearchText)
    }

    fun markNeedRefresh() {
        isNeedRefresh = true
    }

    fun initializePreferences() {
        val prefs = ksuApp.getSharedPreferences("settings", 0)
        _uiState.update {
            it.copy(
                checkModuleUpdate = prefs.getBoolean("module_check_update", true),
                sortEnabledFirst = prefs.getBoolean("module_sort_enabled_first", false),
                sortActionFirst = prefs.getBoolean("module_sort_action_first", false),
            )
        }
        updateModuleList()
    }

    fun toggleSortActionFirst() {
        val newValue = !_uiState.value.sortActionFirst
        ksuApp.getSharedPreferences("settings", 0).edit {
            putBoolean("module_sort_action_first", newValue)
        }
        _uiState.update { it.copy(sortActionFirst = newValue) }
        updateModuleList()
    }

    fun toggleSortEnabledFirst() {
        val newValue = !_uiState.value.sortEnabledFirst
        ksuApp.getSharedPreferences("settings", 0).edit {
            putBoolean("module_sort_enabled_first", newValue)
        }
        _uiState.update { it.copy(sortEnabledFirst = newValue) }
        updateModuleList()
    }

    fun refreshEnvironmentState() {
        viewModelScope.launch {
            val magiskInstalled = withContext(Dispatchers.IO) { hasMagisk() }
            val isSafeMode = Natives.isSafeMode
            _uiState.update {
                it.copy(
                    magiskInstalled = magiskInstalled,
                    isSafeMode = isSafeMode,
                )
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

    private fun filterModules(modules: List<Module>, text: String): List<Module> {
        if (text.isEmpty()) return emptyList()

        return modules.filter {
            it.id.contains(text, true) || it.name.contains(text, true) ||
                    it.description.contains(text, true) || it.author.contains(text, true) ||
                    me.weishu.kernelsu.ui.util.HanziToPinyin.getInstance().toPinyinString(it.name)
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
            updateModuleList()
            return
        }

        val result = withContext(Dispatchers.IO) {
            val state = _uiState.value
            filterModules(state.modules, text).sortedWith(moduleComparator(state))
        }

        _uiState.update {
            it.copy(
                searchResults = result,
                searchStatus = it.searchStatus.copy(
                    resultStatus = searchResultStatusFor(text, result.isEmpty())
                )
            )
        }
    }

    private fun updateModuleList() {
        viewModelScope.launch(Dispatchers.IO) {
            val state = _uiState.value
            val searchText = state.searchStatus.searchText
            val shorted = state.modules.sortedWith(moduleComparator(state))
            val searchResults = filterModules(shorted, searchText)

            _uiState.update {
                it.copy(
                    moduleList = shorted,
                    searchResults = searchResults,
                    searchStatus = it.searchStatus.copy(
                        resultStatus = searchResultStatusFor(searchText, searchResults.isEmpty())
                    )
                )
            }
        }
    }

    private fun moduleComparator(state: ModuleUiState): Comparator<Module> {
        return compareBy<Module>(
            {
                val executable = it.hasWebUi || it.hasActionScript
                when {
                    it.metamodule && it.enabled -> 0
                    state.sortEnabledFirst && state.sortActionFirst -> when {
                        it.enabled && executable -> 1
                        it.enabled -> 2
                        executable -> 3
                        else -> 4
                    }

                    state.sortEnabledFirst && !state.sortActionFirst -> if (it.enabled) 1 else 2
                    !state.sortEnabledFirst && state.sortActionFirst -> if (executable) 1 else 2
                    else -> 1
                }
            },
            { if (state.sortEnabledFirst) !it.enabled else 0 },
            { if (state.sortActionFirst) !(it.hasWebUi || it.hasActionScript) else 0 },
        ).thenBy(Collator.getInstance(Locale.getDefault()), Module::id)
    }

    suspend fun loadModuleList() {
        val parsedModules = withContext(Dispatchers.IO) {
            repo.getModules().getOrElse {
                Log.e(TAG, "fetchModuleList: ", it)
                emptyList()
            }
        }

        withContext(Dispatchers.Main) {
            _uiState.update {
                it.copy(
                    modules = parsedModules,
                )
            }
            // Trigger recalculation of moduleList
            updateModuleList()
            isNeedRefresh = false
        }
    }

    fun fetchModuleList(checkUpdate: Boolean = false) {
        viewModelScope.launch {
            _uiState.update { it.copy(isRefreshing = true) }

            val start = SystemClock.elapsedRealtime()

            loadModuleList()

            if (checkUpdate) syncModuleUpdateInfo(_uiState.value.modules)

            _uiState.update { it.copy(isRefreshing = false) }

            Log.i(TAG, "load cost: ${SystemClock.elapsedRealtime() - start}, modules: ${_uiState.value.modules}")
        }
    }

    private fun Module.toSignature(): ModuleUpdateSignature {
        return ModuleUpdateSignature(
            updateJson = updateJson,
            versionCode = versionCode,
            enabled = enabled,
            update = update,
            remove = remove
        )
    }

    suspend fun syncModuleUpdateInfo(modules: List<Module>) {
        if (!_uiState.value.checkModuleUpdate) return

        val modulesToFetch = mutableListOf<Triple<String, Module, ModuleUpdateSignature>>()
        val removedIds = mutableSetOf<String>()

        updateInfoMutex.withLock {
            val ids = modules.map { it.id }.toSet()
            updateInfoCache.keys.filter { it !in ids }.forEach { removedId ->
                removedIds += removedId
                updateInfoCache.remove(removedId)
                updateInfoInFlight.remove(removedId)
            }

            modules.forEach { module ->
                val signature = module.toSignature()
                val cached = updateInfoCache[module.id]
                if ((cached == null || cached.signature != signature) && updateInfoInFlight.add(module.id)) {
                    modulesToFetch += Triple(module.id, module, signature)
                }
            }
        }

        val fetchedEntries = coroutineScope {
            modulesToFetch.map { (id, module, signature) ->
                async(Dispatchers.IO) {
                    id to ModuleUpdateCache(signature, checkUpdate(module))
                }
            }.awaitAll()
        }

        val changedEntries = mutableListOf<Pair<String, ModuleUpdateInfo>>()
        updateInfoMutex.withLock {
            fetchedEntries.forEach { (id, entry) ->
                val existing = updateInfoCache[id]
                if (existing == null || existing.signature != entry.signature || existing.info != entry.info) {
                    updateInfoCache[id] = entry
                    changedEntries += id to entry.info
                }
                updateInfoInFlight.remove(id)
            }
        }

        if (removedIds.isEmpty() && changedEntries.isEmpty()) {
            return
        }

        withContext(Dispatchers.Main) {
            _uiState.update { state ->
                val newMap = state.updateInfo.toMutableMap()
                removedIds.forEach { newMap.remove(it) }
                changedEntries.forEach { (id, info) ->
                    newMap[id] = info
                }
                state.copy(updateInfo = newMap)
            }
        }
    }

    private suspend fun checkUpdate(m: Module): ModuleUpdateInfo {
        return repo.checkUpdate(m).getOrDefault(ModuleUpdateInfo.Empty)
    }
}
