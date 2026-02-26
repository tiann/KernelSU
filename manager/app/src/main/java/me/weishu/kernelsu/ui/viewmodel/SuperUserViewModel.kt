package me.weishu.kernelsu.ui.viewmodel

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.graphics.drawable.Drawable
import android.os.Build
import android.os.IBinder
import android.os.Parcelable
import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.ipc.RootService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.IKsuInterface
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.KsuService
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.util.HanziToPinyin
import me.weishu.kernelsu.ui.util.KsuCli
import java.text.Collator
import java.util.Locale
import kotlin.coroutines.resume

class SuperUserViewModel : ViewModel() {

    companion object {
        private const val TAG = "SuperUserViewModel"
        private val appsLock = Any()
        private val refreshMutex = Mutex()
        var apps by mutableStateOf<List<AppInfo>>(emptyList())

        @JvmStatic
        fun getAppIconDrawable(context: Context, packageName: String): Drawable? {
            val appList = synchronized(appsLock) { apps }
            val appDetail = appList.find { it.packageName == packageName }
            return appDetail?.packageInfo?.applicationInfo?.loadIcon(context.packageManager)
        }
    }

    private var _appList = mutableStateOf<List<AppInfo>>(emptyList())
    val appList: State<List<AppInfo>> = _appList

    private val _userIds = mutableStateOf<List<Int>>(emptyList())
    val userIds: State<List<Int>> = _userIds

    private val _searchStatus = mutableStateOf(SearchStatus(""))
    val searchStatus: State<SearchStatus> = _searchStatus

    @Parcelize
    data class AppInfo(
        val label: String,
        val packageInfo: PackageInfo,
        val profile: Natives.Profile?,
    ) : Parcelable {
        val packageName: String
            get() = packageInfo.packageName
        val uid: Int
            get() = packageInfo.applicationInfo!!.uid

        val allowSu: Boolean
            get() = profile != null && profile.allowSu
        val hasCustomProfile: Boolean
            get() {
                if (profile == null) {
                    return false
                }

                return if (profile.allowSu) {
                    !profile.rootUseDefault
                } else {
                    !profile.nonRootUseDefault
                }
            }
    }

    var showSystemApps by mutableStateOf(false)
    var showOnlyPrimaryUserApps by mutableStateOf(false)
    var isRefreshing by mutableStateOf(false)
        private set

    var isNeedRefresh by mutableStateOf(false)
        private set

    fun markNeedRefresh() {
        isNeedRefresh = true
    }

    private val _searchResults = mutableStateOf<List<AppInfo>>(emptyList())
    val searchResults: State<List<AppInfo>> = _searchResults

    suspend fun updateSearchText(text: String) {
        _searchStatus.value.searchText = text

        if (text.isEmpty()) {
            _searchStatus.value.resultStatus = SearchStatus.ResultStatus.DEFAULT
            _searchResults.value = emptyList()
            return
        }

        val result = withContext(Dispatchers.IO) {
            _searchStatus.value.resultStatus = SearchStatus.ResultStatus.LOAD
            _appList.value.filter {
                it.label.contains(_searchStatus.value.searchText, true) || it.packageName.contains(
                    _searchStatus.value.searchText,
                    true
                ) || HanziToPinyin.getInstance().toPinyinString(it.label)
                    .contains(_searchStatus.value.searchText, true)
            }
        }

        _searchResults.value = result
        _searchStatus.value.resultStatus = if (result.isEmpty()) {
            SearchStatus.ResultStatus.EMPTY
        } else {
            SearchStatus.ResultStatus.SHOW
        }

    }

    private suspend inline fun connectKsuService(
        crossinline onDisconnect: () -> Unit = {}
    ): Pair<IBinder, ServiceConnection> = withContext(Dispatchers.Main) {
        suspendCancellableCoroutine { cont ->
            val connection = object : ServiceConnection {
                override fun onServiceDisconnected(name: ComponentName?) {
                    onDisconnect()
                }

                override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
                    if (cont.isActive) {
                        cont.resume(binder as IBinder to this)
                    } else {
                        RootService.unbind(this)
                    }
                }
            }

            cont.invokeOnCancellation { RootService.unbind(connection) }

            val intent = Intent(ksuApp, KsuService::class.java)

            val task = RootService.bindOrTask(
                intent,
                Shell.EXECUTOR,
                connection,
            )
            val shell = KsuCli.SHELL
            task?.let { shell.execTask(it) }
        }
    }

    private fun stopKsuService() {
        val intent = Intent(ksuApp, KsuService::class.java)
        RootService.stop(intent)
    }

    private fun filterAndSort(list: List<AppInfo>): List<AppInfo> {
        val comparator = compareBy<AppInfo> {
            when {
                it.allowSu -> 0
                it.hasCustomProfile -> 1
                else -> 2
            }
        }.then(compareBy(Collator.getInstance(Locale.getDefault()), AppInfo::label))
        return list.filter {
            if (it.allowSu || it.hasCustomProfile) {
                return@filter true
            }
            val userFilter = !showOnlyPrimaryUserApps || it.uid / 100000 == 0
            val isSystemApp = it.packageInfo.applicationInfo!!.flags.and(ApplicationInfo.FLAG_SYSTEM) != 0
            val typeFilter = it.uid == 2000
                    || showSystemApps
                    || !isSystemApp
            userFilter && typeFilter
        }.sortedWith(comparator)
    }

    suspend fun fetchAppList() {
        refreshMutex.withLock {
            withContext(Dispatchers.Main) { isRefreshing = true }

            val result = connectKsuService {
                Log.w(TAG, "KsuService disconnected")
            }

            var currentBinder = result.first
            var currentConnection = result.second

            try {
                suspend fun reconnect(): IKsuInterface {
                    RootService.unbind(currentConnection)

                    val retry = connectKsuService { Log.w(TAG, "KsuService disconnected") }
                    currentBinder = retry.first
                    currentConnection = retry.second
                    return IKsuInterface.Stub.asInterface(currentBinder)
                }

                val allPackagesSlice = withContext(Dispatchers.IO) {
                    val pm = ksuApp.packageManager
                    val start = SystemClock.elapsedRealtime()

                    var iface = IKsuInterface.Stub.asInterface(currentBinder)
                    val idsArray = try {
                        iface.userIds
                    } catch (_: Exception) {
                        iface = reconnect()
                        iface.userIds
                    }

                    val slice = try {
                        iface.getPackages(0)
                    } catch (_: Exception) {
                        iface = reconnect()
                        iface.getPackages(0)
                    }

                    val packages = slice.list
                    val newApps = packages.map {
                        val appInfo = it.applicationInfo
                        val uid = appInfo!!.uid
                        val profile = Natives.getAppProfile(it.packageName, uid)
                        AppInfo(
                            label = appInfo.loadLabel(pm).toString(),
                            packageInfo = it,
                            profile = profile,
                        )
                    }.filter {
                        val ai = it.packageInfo.applicationInfo!!
                        if (Build.VERSION.SDK_INT >= 29) !ai.isResourceOverlay else true
                    }

                    val sortedFiltered = filterAndSort(newApps)

                    Log.i(TAG, "load cost: ${SystemClock.elapsedRealtime() - start}")

                    Triple(newApps, sortedFiltered, idsArray)
                }
                withContext(Dispatchers.Main) {
                    synchronized(appsLock) {
                        apps = allPackagesSlice.first
                    }
                    _appList.value = allPackagesSlice.second
                    _userIds.value = allPackagesSlice.third.toList()
                }
            } finally {
                withContext(Dispatchers.Main) {
                    isRefreshing = false
                    isNeedRefresh = false
                }
                RootService.unbind(currentConnection)
            }
        }
    }

    private suspend fun refreshAppList() {
        refreshMutex.withLock {
            val currentApps = synchronized(appsLock) { apps }
            if (currentApps.isEmpty()) return

            val updatedApps = withContext(Dispatchers.IO) {
                currentApps.map {
                    val profile = Natives.getAppProfile(it.packageName, it.uid)
                    it.copy(profile = profile)
                }
            }

            val sortedFiltered = withContext(Dispatchers.IO) {
                filterAndSort(updatedApps)
            }

            withContext(Dispatchers.Main) {
                synchronized(appsLock) {
                    apps = updatedApps
                }
                _appList.value = sortedFiltered
                isNeedRefresh = false
            }
        }
    }

    fun loadAppList(force: Boolean = false) {
        viewModelScope.launch {
            if (force || apps.isEmpty()) {
                fetchAppList()
            } else {
                refreshAppList()
            }
        }
    }
}
