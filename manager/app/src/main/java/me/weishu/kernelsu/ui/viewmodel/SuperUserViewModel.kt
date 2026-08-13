package me.weishu.kernelsu.ui.viewmodel

import android.content.pm.ApplicationInfo
import android.util.Log
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.data.repository.SettingsRepository
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl
import me.weishu.kernelsu.data.repository.SuperUserRepository
import me.weishu.kernelsu.data.repository.SuperUserRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.screen.superuser.GroupedApps
import me.weishu.kernelsu.ui.screen.superuser.SuperUserUiState
import me.weishu.kernelsu.ui.util.PinyinUtil
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.util.pickPrimary
import java.text.Collator
import java.util.Locale

internal const val RECENTLY_INSTALLED_WINDOW_MILLIS = 60 * 60 * 1000L

enum class AppSortType {
    NAME, PACKAGE_NAME, INSTALL_TIME, UPDATE_TIME;

    companion object {
        fun fromOrdinal(ordinal: Int): AppSortType =
            entries.getOrElse(ordinal) { NAME }
    }
}

data class AppSortConfig(
    val sortType: AppSortType = AppSortType.NAME,
    val reversed: Boolean = false,
) {
    fun toInt(): Int = sortType.ordinal * 2 + if (reversed) 1 else 0

    fun withType(type: AppSortType): AppSortConfig = copy(sortType = type)
    fun toggleReversed(): AppSortConfig = copy(reversed = !reversed)

    companion object {
        fun fromInt(value: Int): AppSortConfig = AppSortConfig(
            sortType = AppSortType.fromOrdinal(value / 2),
            reversed = value % 2 != 0,
        )
    }
}

internal fun buildRecentlyInstalledGroups(
    groups: List<GroupedApps>,
    nowMillis: Long = System.currentTimeMillis(),
): List<GroupedApps> {
    val cutoffMillis = nowMillis - RECENTLY_INSTALLED_WINDOW_MILLIS
    val collator = Collator.getInstance(Locale.getDefault())

    return groups.mapNotNull { group ->
        val latestInstallTime = group.apps.maxOfOrNull { it.packageInfo.firstInstallTime } ?: return@mapNotNull null
        if (latestInstallTime < cutoffMillis) {
            null
        } else {
            group to latestInstallTime
        }
    }.sortedWith(
        compareByDescending<Pair<GroupedApps, Long>> { it.second }
            .thenBy(collator) { it.first.primary.label }
    ).map { it.first }
}

class SuperUserViewModel(
    private val repo: SuperUserRepository = SuperUserRepositoryImpl(),
    private val settingsRepo: SettingsRepository = SettingsRepositoryImpl()
) : ViewModel() {

    companion object {
        private const val TAG = "SuperUserViewModel"

        private val appsLock = Any()
        private var cachedApps: List<AppInfo> = emptyList()
        private val groupedAppsLock = Any()
        private var cachedGroupedApps: List<GroupedApps> = emptyList()

        val apps: List<AppInfo>
            get() = synchronized(appsLock) { cachedApps }

        @JvmStatic
        fun getGroupedApp(uid: Int): GroupedApps? {
            return synchronized(groupedAppsLock) { cachedGroupedApps.find { it.uid == uid } }
        }
    }

    typealias AppInfo = me.weishu.kernelsu.data.model.AppInfo

    private val _uiState = MutableStateFlow(SuperUserUiState())
    val uiState: StateFlow<SuperUserUiState> = _uiState.asStateFlow()

    private val refreshMutex = Mutex()
    private val searchQuery = MutableStateFlow("")
    private var sortJob: Job? = null
    var isNeedRefresh = false
        private set

    init {
        viewModelScope.launchSearchQueryCollector(searchQuery, ::applySearchText)
    }

    fun markNeedRefresh() {
        isNeedRefresh = true
    }

    fun initializePreferences() {
        _uiState.update {
            it.copy(
                showSystemApps = settingsRepo.superuserShowSystemApps,
                showOnlyPrimaryUserApps = settingsRepo.superuserShowOnlyPrimaryUserApps,
                sortConfig = AppSortConfig.fromInt(settingsRepo.superuserSortOption),
            )
        }
    }

    fun updateSortConfig(config: AppSortConfig): Job {
        settingsRepo.superuserSortOption = config.toInt()
        _uiState.update { it.copy(sortConfig = config) }
        sortJob?.cancel()
        return viewModelScope.launch {
            val current = _uiState.value.groupedApps
            if (current.isEmpty()) return@launch
            updateVisibleApps(current)
        }.also { sortJob = it }
    }

    private fun refilterVisibleApps(): Job = viewModelScope.launch {
        // Re-filter when a filter setting changes
        val grouped = withContext(Dispatchers.IO) {
            buildGroups(filterApps(apps))
        }
        updateVisibleApps(grouped)
    }

    fun toggleShowSystemApps(): Job {
        val newValue = !_uiState.value.showSystemApps
        settingsRepo.superuserShowSystemApps = newValue
        _uiState.update { it.copy(showSystemApps = newValue) }
        return refilterVisibleApps()
    }

    fun toggleShowOnlyPrimaryUserApps(): Job {
        val newValue = !_uiState.value.showOnlyPrimaryUserApps
        settingsRepo.superuserShowOnlyPrimaryUserApps = newValue
        _uiState.update { it.copy(showOnlyPrimaryUserApps = newValue) }
        return refilterVisibleApps()
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

    private fun filterSearchResults(groups: List<GroupedApps>, text: String): List<GroupedApps> {
        if (text.isEmpty()) return emptyList()

        return groups.mapNotNull { group ->
            val matchedPackageNames = group.apps.filter {
                it.label.contains(text, true) ||
                        it.packageName.contains(text, true) ||
                        PinyinUtil.toPinyin(it.label).contains(text, true)
            }.mapTo(linkedSetOf()) { it.packageName }

            if (matchedPackageNames.isEmpty()) {
                null
            } else {
                val sortedApps = group.apps.sortedWith(
                    compareByDescending { it.packageName in matchedPackageNames }
                )
                group.copy(
                    apps = sortedApps,
                    matchedPackageNames = matchedPackageNames,
                )
            }
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
            filterSearchResults(_uiState.value.groupedApps, text)
        }

        _uiState.update {
            it.copy(
                searchResults = result,
                searchStatus = it.searchStatus.copy(resultStatus = searchResultStatusFor(text, result.isEmpty()))
            )
        }
    }

    private fun updateCachedGroupedApps(grouped: List<GroupedApps>) {
        synchronized(groupedAppsLock) {
            cachedGroupedApps = grouped
        }
    }

    private suspend fun updateVisibleApps(grouped: List<GroupedApps>, resort: Boolean = true) {
        val sortConfig = _uiState.value.sortConfig
        val searchText = _uiState.value.searchStatus.searchText
        val (sorted, searchResults, recentlyInstalledResults) = withContext(Dispatchers.IO) {
            val s = if (resort) sortGroups(grouped, sortConfig) else grouped
            Triple(s, filterSearchResults(s, searchText), buildRecentlyInstalledGroups(s))
        }
        _uiState.update {
            it.copy(
                groupedApps = sorted,
                recentlyInstalledResults = recentlyInstalledResults,
                searchResults = searchResults,
                searchStatus = it.searchStatus.copy(
                    resultStatus = searchResultStatusFor(searchText, searchResults.isEmpty())
                )
            )
        }
    }

    private fun filterApps(list: List<AppInfo>): List<AppInfo> {
        val currentState = _uiState.value

        return list.filter {
            if (it.packageName == ksuApp.packageName) return@filter false
            if (it.allowSu || it.hasCustomProfile) {
                return@filter true
            }
            val userFilter = !currentState.showOnlyPrimaryUserApps || it.uid / 100000 == 0
            val isSystemApp = it.packageInfo.applicationInfo!!.flags.and(ApplicationInfo.FLAG_SYSTEM) != 0
            val typeFilter = it.uid == 2000
                    || currentState.showSystemApps
                    || !isSystemApp
            userFilter && typeFilter
        }
    }

    private fun buildCachedGroups(apps: List<AppInfo>): List<GroupedApps> {
        return buildGroups(apps.filter { it.packageName != ksuApp.packageName })
    }

    private fun buildGroups(
        apps: List<AppInfo>,
        umount: (Int) -> Boolean = { Natives.uidShouldUmount(it) },
    ): List<GroupedApps> {
        val collator = Collator.getInstance(Locale.getDefault())
        val comparator = compareBy<AppInfo> {
            when {
                it.allowSu -> 0
                it.hasCustomProfile -> 1
                else -> 2
            }
        }.thenBy(collator) { it.label }
        return apps.groupBy { it.uid }.map { (uid, list) ->
            val sorted = list.sortedWith(comparator)
            val primary = pickPrimary(sorted)
            val shouldUmount = umount(uid)
            val ownerName = if (sorted.size > 1) ownerNameForUid(uid, sorted) else null

            GroupedApps(
                uid = uid,
                apps = sorted,
                primary = primary,
                anyAllowSu = sorted.any { it.allowSu },
                anyCustom = sorted.any { it.hasCustomProfile },
                shouldUmount = shouldUmount,
                ownerName = ownerName
            )
        }
    }

    private fun groupRank(group: GroupedApps): Int = when {
        group.anyAllowSu -> 0
        group.anyCustom -> 1
        group.apps.size > 1 -> 2
        else -> 3
    }

    private fun sortGroups(groups: List<GroupedApps>, config: AppSortConfig): List<GroupedApps> {
        val collator = Collator.getInstance(Locale.getDefault())
        val base: Comparator<GroupedApps> = when (config.sortType) {
            AppSortType.PACKAGE_NAME -> compareBy { it.primary.packageName }
            AppSortType.INSTALL_TIME -> compareBy { it.primary.packageInfo.firstInstallTime }
            AppSortType.UPDATE_TIME -> compareBy { it.primary.packageInfo.lastUpdateTime }
            AppSortType.NAME -> Comparator { a, b -> collator.compare(a.primary.label, b.primary.label) }
        }
        val secondary = if (config.reversed) base.reversed() else base

        return groups.sortedWith(Comparator { a, b ->
            val ra = groupRank(a)
            val rb = groupRank(b)
            if (ra != rb) ra - rb else secondary.compare(a, b)
        })
    }

    suspend fun fetchAppList() {
        refreshMutex.withLock {
            _uiState.update { it.copy(isRefreshing = true, error = null) }

            repo.getAppList().onSuccess { (newApps, ids) ->
                val (cachedGroups, grouped) = withContext(Dispatchers.IO) {
                    val cached = buildCachedGroups(newApps)
                    val umountByUid = cached.associate { it.uid to it.shouldUmount }
                    cached to buildGroups(filterApps(newApps)) { umountByUid[it] ?: Natives.uidShouldUmount(it) }
                }

                // Update cache for static method
                synchronized(appsLock) { cachedApps = newApps }
                updateCachedGroupedApps(cachedGroups)
                updateVisibleApps(grouped)
                _uiState.update { it.copy(userIds = ids, isRefreshing = false, hasLoaded = true) }
            }.onFailure { e ->
                Log.e(TAG, "fetchAppList failed", e)
                _uiState.update {
                    it.copy(
                        isRefreshing = false,
                        hasLoaded = true,
                        error = e
                    )
                }
            }

            isNeedRefresh = false
        }
    }

    private suspend fun refreshAppList(resort: Boolean = true) {
        refreshMutex.withLock {
            val currentApps = synchronized(appsLock) { cachedApps }
            if (currentApps.isEmpty()) return

            repo.refreshProfiles(currentApps).onSuccess { updatedApps ->
                // Update cache for static method
                synchronized(appsLock) { cachedApps = updatedApps }

                val (cachedGroups, grouped) = withContext(Dispatchers.IO) {
                    val cached = buildCachedGroups(updatedApps)
                    val umountByUid = cached.associate { it.uid to it.shouldUmount }
                    val visible = buildGroups(filterApps(updatedApps)) {
                        umountByUid[it] ?: Natives.uidShouldUmount(it)
                    }
                    val result = if (resort) {
                        visible
                    } else {
                        val byUid = visible.associateBy { it.uid }
                        _uiState.value.groupedApps.map { group ->
                            byUid[group.uid] ?: group.copy(shouldUmount = Natives.uidShouldUmount(group.uid))
                        }
                    }
                    cached to result
                }
                updateCachedGroupedApps(cachedGroups)

                updateVisibleApps(grouped, resort = resort)
                _uiState.update { it.copy(isRefreshing = false) }
                isNeedRefresh = false
            }
        }
    }

    fun loadAppList(force: Boolean = false, resort: Boolean = true): Job {
        return viewModelScope.launch {
            if (force || _uiState.value.groupedApps.isEmpty()) {
                fetchAppList()
            } else {
                refreshAppList(resort)
            }
        }
    }

}
