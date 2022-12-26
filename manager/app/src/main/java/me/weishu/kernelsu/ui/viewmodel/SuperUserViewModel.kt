package me.weishu.kernelsu.ui.viewmodel

import android.graphics.drawable.Drawable
import android.util.Log
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ksuApp
import java.text.Collator
import java.util.*

class SuperUserViewModel : ViewModel() {

    companion object {
        private const val TAG = "SuperUserViewModel"
        private var apps by mutableStateOf<List<AppInfo>>(emptyList())
    }

    class AppInfo(
        val label: String,
        val packageName: String,
        val icon: Drawable,
        val uid: Int,
        val onAllowList: Boolean,
        val onDenyList: Boolean
    )

    var search by mutableStateOf("")
    var isRefreshing by mutableStateOf(false)
        private set

    private val sortedList by derivedStateOf {
        val comparator = compareBy<AppInfo> {
            when {
                it.onAllowList -> 0
                it.onDenyList -> 1
                else -> 2
            }
        }.then(compareBy(Collator.getInstance(Locale.getDefault()), AppInfo::label))
        apps.sortedWith(comparator).also {
            isRefreshing = false
        }
    }

    val appList by derivedStateOf {
        sortedList.filter {
            it.label.contains(search) || it.packageName.contains(search)
        }
    }

    suspend fun fetchAppList() {
        withContext(Dispatchers.IO) {
            isRefreshing = true
            val pm = ksuApp.packageManager
            val allowList = Natives.getAllowList().toSet()
            val denyList = Natives.getDenyList().toSet()
            Log.i(TAG, "allowList: $allowList")
            Log.i(TAG, "denyList: $denyList")
            apps = pm.getInstalledApplications(0).map {
                AppInfo(
                    label = it.loadLabel(pm).toString(),
                    packageName = it.packageName,
                    icon = it.loadIcon(pm),
                    uid = it.uid,
                    onAllowList = it.uid in allowList,
                    onDenyList = it.uid in denyList
                )
            }
        }
    }
}
