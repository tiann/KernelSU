package me.weishu.kernelsu.ui.viewmodel

import android.net.Uri
import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.ShellUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ksuApp
import org.json.JSONArray
import java.io.File
import java.text.Collator
import java.util.*

class ModuleViewModel : ViewModel() {

    companion object {
        private const val TAG = "ModuleViewModel"
        private var modules by mutableStateOf<List<ModuleInfo>>(emptyList())
    }

    class ModuleInfo(
        val id: String,
        val name: String,
        val author: String,
        val version: String,
        val versionCode: Int,
        val description: String,
        val enabled: Boolean
    )

    var isRefreshing by mutableStateOf(false)
        private set

    val moduleList by derivedStateOf {
        val comparator = compareBy(Collator.getInstance(Locale.getDefault()), ModuleInfo::id)
        modules.sortedWith(comparator).also {
            isRefreshing = false
        }
    }

    suspend fun fetchModuleList() {
        withContext(Dispatchers.IO) {
            isRefreshing = true
            val start = SystemClock.elapsedRealtime()

            val shell = ksuApp.createRootShell()
            val ksduLib = ksuApp.applicationInfo.nativeLibraryDir + File.separator + "libksud.so"

            val out = shell.newJob().add("$ksduLib module list").to(ArrayList(), null).exec().out
            val result = out.joinToString("\n")

            Log.i(TAG, "result: $result")

            kotlin.runCatching {
                val array = JSONArray(result)
                modules = (0 until array.length())
                    .asSequence()
                    .map { array.getJSONObject(it) }
                    .map { obj ->
                        ModuleInfo(
                            obj.getString("id"),
                            obj.getString("name"),
                            obj.getString("author"),
                            obj.getString("version"),
                            obj.getInt("versionCode"),
                            obj.getString("description"),
                            obj.getBoolean("enabled")
                        )
                    }.toList()
            }.onFailure { e ->
                Log.e(TAG, "fetchModuleList: ", e)
            }


            Log.i(TAG, "load cost: ${SystemClock.elapsedRealtime() - start}, modules: $modules")
        }
    }
}
