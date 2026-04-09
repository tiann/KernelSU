package me.weishu.kernelsu.ui.viewmodel

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.drawable.Drawable
import android.util.Log
import androidx.core.content.edit
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
import me.weishu.kernelsu.data.repository.SuperUserRepository
import me.weishu.kernelsu.data.repository.SuperUserRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.screen.superuser.GroupedApps
import me.weishu.kernelsu.ui.screen.superuser.SuperUserUiState
import me.weishu.kernelsu.ui.util.HanziToPinyin
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.util.pickPrimary
import me.weishu.kernelsu.ui.util.withCurrentUserUid
import java.text.Collator
import java.util.Locale

class SuperUserViewModel(
    private val repo: SuperUserRepository = SuperUserRepositoryImpl()
) : ViewModel() {

    companion object {
        private const val TAG = "SuperUserViewModel"

        // Cache to support getAppIconDrawable static method
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

        @JvmStatic
        fun getAppIconDrawable(context: Context, packageName: String): Drawable? {
            val appList = synchronized(appsLock) { cachedApps }
            val appDetail = appList.find { it.packageName == packageName }
            val appInfo = appDetail?.packageInfo?.applicationInfo ?: return null
            return appInfo.withCurrentUserUid().loadIcon(context.packageManager)
        }
    }

    typealias AppInfo = me.weishu.kernelsu.data.model.AppInfo

    private val _uiState = MutableStateFlow(SuperUserUiState())
    val uiState: StateFlow<SuperUserUiState> = _uiState.asStateFlow()
    private val prefs = ksuApp.getSharedPreferences("settings", Context.MODE_PRIVATE)

    private val refreshMutex = Mutex()
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
        val showSystemApps = prefs.getBoolean("show_system_apps", false)
        val showOnlyPrimaryUserApps = prefs.getBoolean("show_only_primary_user_apps", false)
        _uiState.update {
            it.copy(
                showSystemApps = showSystemApps,
                showOnlyPrimaryUserApps = showOnlyPrimaryUserApps,
            )
        }
    }

    fun toggleShowSystemApps(): Job {
        val newValue = !_uiState.value.showSystemApps
        prefs.edit { putBoolean("show_system_apps", newValue) }
        _uiState.update { it.copy(showSystemApps = newValue) }
        return viewModelScope.launch {
            // Re-filter when setting changes
            val grouped = withContext(Dispatchers.IO) {
                buildGroups(filterAndSort(apps))
            }
            updateVisibleApps(grouped)
        }
    }

    fun toggleShowOnlyPrimaryUserApps(): Job {
        val newValue = !_uiState.value.showOnlyPrimaryUserApps
        prefs.edit { putBoolean("show_only_primary_user_apps", newValue) }
        _uiState.update { it.copy(showOnlyPrimaryUserApps = newValue) }
        return viewModelScope.launch {
            // Re-filter when setting changes
            val grouped = withContext(Dispatchers.IO) {
                buildGroups(filterAndSort(apps))
            }
            updateVisibleApps(grouped)
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

    private fun filterSearchResults(groups: List<GroupedApps>, text: String): List<GroupedApps> {
        if (text.isEmpty()) return emptyList()

        return groups.mapNotNull { group ->
            val matchedPackageNames = group.apps.filter {
                it.label.contains(text, true) ||
                        it.packageName.contains(text, true) ||
                        HanziToPinyin.getInstance().toPinyinString(it.label).contains(text, true)
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
            cachedGroupedApps = grouped.map { it.copy(matchedPackageNames = emptySet()) }
        }
    }

    private fun updateVisibleApps(grouped: List<GroupedApps>) {
        val searchText = _uiState.value.searchStatus.searchText
        val searchResults = filterSearchResults(grouped, searchText)
        _uiState.update {
            it.copy(
                groupedApps = grouped.map { group -> group.copy(matchedPackageNames = emptySet()) },
                searchResults = searchResults,
                searchStatus = it.searchStatus.copy(
                    resultStatus = searchResultStatusFor(searchText, searchResults.isEmpty())
                )
            )
        }
    }

    private fun filterAndSort(list: List<AppInfo>): List<AppInfo> {
        val comparator = compareBy<AppInfo> {
            when {
                it.allowSu -> 0
                it.hasCustomProfile -> 1
                else -> 2
            }
        }.then(compareBy(Collator.getInstance(Locale.getDefault()), AppInfo::label))

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
        }.sortedWith(comparator)
    }

    private fun buildCachedGroups(apps: List<AppInfo>): List<GroupedApps> {
        return buildGroups(apps.filter { it.packageName != ksuApp.packageName })
    }

    private fun buildGroups(apps: List<AppInfo>): List<GroupedApps> {
        val comparator = compareBy<AppInfo> {
            when {
                it.allowSu -> 0
                it.hasCustomProfile -> 1
                else -> 2
            }
        }.thenBy { it.label.lowercase() }
        val groups = apps.groupBy { it.uid }.map { (uid, list) ->
            val sorted = list.sortedWith(comparator)
            val primary = pickPrimary(sorted)
            val shouldUmount = Natives.uidShouldUmount(uid)
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
        return groups.sortedWith(Comparator { a, b ->
            fun rank(g: GroupedApps): Int = when {
                g.anyAllowSu -> 0
                g.anyCustom -> 1
                g.apps.size > 1 -> 2
                g.shouldUmount -> 4
                else -> 3
            }

            val ra = rank(a)
            val rb = rank(b)
            if (ra != rb) return@Comparator ra - rb
            return@Comparator when (ra) {
                2 -> a.uid.compareTo(b.uid)
                else -> a.primary.label.lowercase().compareTo(b.primary.label.lowercase())
            }
        })
    }

    suspend fun fetchAppList() {
        refreshMutex.withLock {
            _uiState.update { it.copy(isRefreshing = true, error = null) }

            repo.getAppList().onSuccess { (newApps, ids) ->
                val (cachedGroups, grouped) = withContext(Dispatchers.IO) {
                    buildCachedGroups(newApps) to buildGroups(filterAndSort(newApps))
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

                val cachedGroups = withContext(Dispatchers.IO) {
                    buildCachedGroups(updatedApps)
                }
                updateCachedGroupedApps(cachedGroups)

                val grouped = if (resort) {
                    withContext(Dispatchers.IO) {
                        buildGroups(filterAndSort(updatedApps))
                    }
                } else {
                    val updatedGroups = buildGroups(filterAndSort(updatedApps)).associateBy { it.uid }
                    _uiState.value.groupedApps.map { group ->
                        val newApps = updatedGroups[group.uid]?.apps ?: group.apps
                        val primary = pickPrimary(newApps)
                        val shouldUmount = Natives.uidShouldUmount(group.uid)
                        val ownerName = if (newApps.size > 1) ownerNameForUid(group.uid, newApps) else null
                        group.copy(
                            apps = newApps,
                            primary = primary,
                            anyAllowSu = newApps.any { it.allowSu },
                            anyCustom = newApps.any { it.hasCustomProfile },
                            shouldUmount = shouldUmount,
                            ownerName = ownerName
                        )
                    }
                }

                updateVisibleApps(grouped)
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
