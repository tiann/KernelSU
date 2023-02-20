package me.weishu.kernelsu.ui.viewmodel

import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ui.util.listModules
import org.json.JSONArray
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
        val enabled: Boolean,
        val update: Boolean,
        val remove: Boolean,
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

            kotlin.runCatching {

                val result = listModules()

                Log.i(TAG, "result: $result")

                val array = JSONArray(result)
                modules = (0 until array.length())
                    .asSequence()
                    .map { array.getJSONObject(it) }
                    .map { obj ->
                        ModuleInfo(
                            obj.getString("id"),
                            obj.getString("name"),
                            obj.optString("author", "Unknown"),
                            obj.optString("version", "Unknown"),
                            obj.optInt("versionCode", 0),
                            obj.getString("description"),
                            obj.getBoolean("enabled"),
                            obj.getBoolean("update"),
                            obj.getBoolean("remove"),
                        )
                    }.toList()
            }.onFailure { e ->
                Log.e(TAG, "fetchModuleList: ", e)
                isRefreshing = false
            }


            Log.i(TAG, "load cost: ${SystemClock.elapsedRealtime() - start}, modules: $modules")
        }
    }
}
