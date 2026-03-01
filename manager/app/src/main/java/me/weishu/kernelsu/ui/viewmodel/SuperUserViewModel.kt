package me.weishu.kernelsu.ui.viewmodel

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.drawable.Drawable
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
import me.weishu.kernelsu.data.repository.SuperUserRepository
import me.weishu.kernelsu.data.repository.SuperUserRepositoryImpl
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.util.HanziToPinyin
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.util.pickPrimary
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

        val apps: List<AppInfo>
            get() = synchronized(appsLock) { cachedApps }

        @JvmStatic
        fun getAppIconDrawable(context: Context, packageName: String): Drawable? {
            val appList = synchronized(appsLock) { cachedApps }
            val appDetail = appList.find { it.packageName == packageName }
            return appDetail?.packageInfo?.applicationInfo?.loadIcon(context.packageManager)
        }
    }

    typealias AppInfo = me.weishu.kernelsu.data.model.AppInfo

    private val _uiState = MutableStateFlow(SuperUserUiState())
    val uiState: StateFlow<SuperUserUiState> = _uiState.asStateFlow()

    private val refreshMutex = Mutex()
    var isNeedRefresh = false
        private set

    fun markNeedRefresh() {
        isNeedRefresh = true
    }

    fun setShowSystemApps(show: Boolean): Job {
        _uiState.update { it.copy(showSystemApps = show) }
        return viewModelScope.launch {
            // Re-filter when setting changes
            val (filtered, grouped) = withContext(Dispatchers.IO) {
                val list = filterAndSort(apps)
                list to buildGroups(list)
            }
            _uiState.update { it.copy(appList = filtered, groupedApps = grouped) }
        }
    }

    fun setShowOnlyPrimaryUserApps(show: Boolean): Job {
        _uiState.update { it.copy(showOnlyPrimaryUserApps = show) }
        return viewModelScope.launch {
            // Re-filter when setting changes
            val (filtered, grouped) = withContext(Dispatchers.IO) {
                val list = filterAndSort(apps)
                list to buildGroups(list)
            }
            _uiState.update { it.copy(appList = filtered, groupedApps = grouped) }
        }
    }

    fun updateSearchStatus(status: SearchStatus) {
        _uiState.update { it.copy(searchStatus = status) }
    }

    suspend fun updateSearchText(text: String) {
        _uiState.update {
            it.copy(
                searchStatus = it.searchStatus.copy(searchText = text)
            )
        }

        if (text.isEmpty()) {
            _uiState.update {
                it.copy(
                    searchStatus = it.searchStatus.copy(resultStatus = SearchStatus.ResultStatus.DEFAULT),
                    searchResults = emptyList()
                )
            }
            return
        }

        _uiState.update {
            it.copy(searchStatus = it.searchStatus.copy(resultStatus = SearchStatus.ResultStatus.LOAD))
        }

        val result = withContext(Dispatchers.IO) {
            _uiState.value.appList.filter {
                it.label.contains(text, true) || it.packageName.contains(
                    text,
                    true
                ) || HanziToPinyin.getInstance().toPinyinString(it.label)
                    .contains(text, true)
            }
        }

        _uiState.update {
            it.copy(
                searchResults = result,
                searchStatus = it.searchStatus.copy(
                    resultStatus = if (result.isEmpty()) SearchStatus.ResultStatus.EMPTY else SearchStatus.ResultStatus.SHOW
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
                val (sortedFiltered, grouped) = withContext(Dispatchers.IO) {
                    val list = filterAndSort(newApps)
                    list to buildGroups(list)
                }

                // Update cache for static method
                synchronized(appsLock) { cachedApps = newApps }

                _uiState.update {
                    it.copy(
                        appList = sortedFiltered,
                        groupedApps = grouped,
                        userIds = ids,
                        isRefreshing = false
                    )
                }
            }.onFailure { e ->
                Log.e(TAG, "fetchAppList failed", e)
                _uiState.update {
                    it.copy(
                        isRefreshing = false,
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

                val (sortedFiltered, grouped) = if (resort) {
                    withContext(Dispatchers.IO) {
                        val list = filterAndSort(updatedApps)
                        list to buildGroups(list)
                    }
                } else {
                    val updatedMap = updatedApps.associateBy { it.packageName }
                    val currentFiltered = _uiState.value.appList.map { updatedMap[it.packageName] ?: it }
                    val currentGroups = _uiState.value.groupedApps.map { group ->
                        val newApps = group.apps.map { updatedMap[it.packageName] ?: it }
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
                    currentFiltered to currentGroups
                }

                _uiState.update {
                    it.copy(
                        appList = sortedFiltered,
                        groupedApps = grouped,
                        isRefreshing = false
                    )
                }
                isNeedRefresh = false
            }
        }
    }

    fun loadAppList(force: Boolean = false, resort: Boolean = true): Job {
        return viewModelScope.launch {
            if (force || _uiState.value.appList.isEmpty()) {
                fetchAppList()
            } else {
                refreshAppList(resort)
            }
        }
    }
}
