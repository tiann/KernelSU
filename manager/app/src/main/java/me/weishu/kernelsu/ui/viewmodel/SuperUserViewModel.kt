package me.weishu.kernelsu.ui.viewmodel

import android.content.ComponentName
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.os.IBinder
import android.os.Parcelable
import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
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
import kotlin.coroutines.suspendCoroutine

class SuperUserViewModel : ViewModel() {

    companion object {
        private const val TAG = "SuperUserViewModel"
        private var apps by mutableStateOf<List<AppInfo>>(emptyList())
    }


    private var _appList = mutableStateOf<List<AppInfo>>(emptyList())
    val appList: State<List<AppInfo>> = _appList
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
    var isRefreshing by mutableStateOf(false)
        private set

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

        if (_searchResults.value == result) {
            fetchAppList()
            updateSearchText(text)
        } else {
            _searchResults.value = result

        }
        _searchStatus.value.resultStatus = if (result.isEmpty()) {
            SearchStatus.ResultStatus.EMPTY
        } else {
            SearchStatus.ResultStatus.SHOW
        }

    }

    private suspend inline fun connectKsuService(
        crossinline onDisconnect: () -> Unit = {}
    ): Pair<IBinder, ServiceConnection> = suspendCoroutine {
        val connection = object : ServiceConnection {
            override fun onServiceDisconnected(name: ComponentName?) {
                onDisconnect()
            }

            override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
                it.resume(binder as IBinder to this)
            }
        }

        val intent = Intent(ksuApp, KsuService::class.java)

        val task = KsuService.bindOrTask(
            intent,
            Shell.EXECUTOR,
            connection,
        )
        val shell = KsuCli.SHELL
        task?.let { it1 -> shell.execTask(it1) }
    }

    private fun stopKsuService() {
        val intent = Intent(ksuApp, KsuService::class.java)
        KsuService.stop(intent)
    }

    suspend fun fetchAppList() {

        isRefreshing = true

        val result = connectKsuService {
            Log.w(TAG, "KsuService disconnected")
        }

        withContext(Dispatchers.IO) {
            val pm = ksuApp.packageManager
            val start = SystemClock.elapsedRealtime()

            val binder = result.first
            val allPackages = IKsuInterface.Stub.asInterface(binder).getPackages(0)

            withContext(Dispatchers.Main) {
                stopKsuService()
            }

            val packages = allPackages.list

            apps = packages.map {
                val appInfo = it.applicationInfo
                val uid = appInfo!!.uid
                val profile = Natives.getAppProfile(it.packageName, uid)
                AppInfo(
                    label = appInfo.loadLabel(pm).toString(),
                    packageInfo = it,
                    profile = profile,
                )
            }.filter { it.packageName != ksuApp.packageName }


            val comparator = compareBy<AppInfo> {
                when {
                    it.allowSu -> 0
                    it.hasCustomProfile -> 1
                    else -> 2
                }
            }.then(compareBy(Collator.getInstance(Locale.getDefault()), AppInfo::label))
            _appList.value = apps.sortedWith(comparator).also {
                isRefreshing = false
            }.filter {
                it.uid == 2000 // Always show shell
                        || showSystemApps || it.packageInfo.applicationInfo!!.flags.and(ApplicationInfo.FLAG_SYSTEM) == 0
            }
            Log.i(TAG, "load cost: ${SystemClock.elapsedRealtime() - start}")
        }
    }
}
