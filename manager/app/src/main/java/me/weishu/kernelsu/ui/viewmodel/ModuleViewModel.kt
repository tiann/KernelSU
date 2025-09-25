package me.weishu.kernelsu.ui.viewmodel

import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.*
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.util.HanziToPinyin
import me.weishu.kernelsu.ui.util.listModules
import me.weishu.kernelsu.ui.util.overlayFsAvailable
import org.json.JSONArray
import org.json.JSONObject
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
        val updateJson: String,
        val hasWebUi: Boolean,
        val hasActionScript: Boolean,
    )

    var isRefreshing by mutableStateOf(false)
        private set

    var isOverlayAvailable by mutableStateOf(overlayFsAvailable())
        private set

    var sortEnabledFirst by mutableStateOf(false)
    var sortActionFirst by mutableStateOf(false)

    private val _searchStatus = mutableStateOf(SearchStatus(""))
    val searchStatus: State<SearchStatus> = _searchStatus

    private val _searchResults = mutableStateOf<List<ModuleInfo>>(emptyList())
    val searchResults: State<List<ModuleInfo>> = _searchResults

    val moduleList by derivedStateOf {
        val comparator =
            compareBy<ModuleInfo>(
                { if (sortEnabledFirst) !it.enabled else 0 },
                { if (sortActionFirst) !it.hasWebUi && !it.hasActionScript else 0 },
            ).thenBy(Collator.getInstance(Locale.getDefault()), ModuleInfo::id)
        modules.filter {
            it.id.contains(searchStatus.value.searchText, true) || it.name.contains(
                searchStatus.value.searchText,
                true
            ) || HanziToPinyin.getInstance()
                .toPinyinString(it.name).contains(searchStatus.value.searchText, true)
        }.sortedWith(comparator).also {
            isRefreshing = false
        }
    }

    var isNeedRefresh by mutableStateOf(false)
        private set

    fun markNeedRefresh() {
        isNeedRefresh = true
    }

    suspend fun updateSearchText(text: String) {
        _searchStatus.value.searchText = text

        if (text.isEmpty()) {
            _searchStatus.value.resultStatus = SearchStatus.ResultStatus.DEFAULT
            _searchResults.value = emptyList()
            return
        }

        val result = withContext(Dispatchers.IO) {
            _searchStatus.value.resultStatus = SearchStatus.ResultStatus.LOAD
            modules.filter {
                it.id.contains(text, true) || it.name.contains(text, true) ||
                        it.description.contains(text, true) || it.author.contains(text, true) ||
                        HanziToPinyin.getInstance().toPinyinString(it.name).contains(text, true)
            }.let { filteredModules ->
                val comparator = compareBy<ModuleInfo>(
                    { if (sortEnabledFirst) !it.enabled else 0 },
                    { if (sortActionFirst) !it.hasWebUi && !it.hasActionScript else 0 },
                ).thenBy(Collator.getInstance(Locale.getDefault()), ModuleInfo::id)
                filteredModules.sortedWith(comparator)
            }
        }

        _searchResults.value = result
        _searchStatus.value.resultStatus = if (result.isEmpty()) {
            SearchStatus.ResultStatus.EMPTY
        } else {
            SearchStatus.ResultStatus.SHOW
        }
    }

    fun fetchModuleList() {
        viewModelScope.launch(Dispatchers.IO) {
            isRefreshing = true

            val oldModuleList = modules

            val start = SystemClock.elapsedRealtime()

            kotlin.runCatching {
                isOverlayAvailable = overlayFsAvailable()

                val result = listModules()

                Log.i(TAG, "result: $result")

                val array = JSONArray(result)
                modules = (0 until array.length())
                    .asSequence()
                    .map { array.getJSONObject(it) }
                    .map { obj ->
                        ModuleInfo(
                            obj.getString("id"),
                            obj.optString("name"),
                            obj.optString("author", "Unknown"),
                            obj.optString("version", "Unknown"),
                            obj.optInt("versionCode", 0),
                            obj.optString("description"),
                            obj.getBoolean("enabled"),
                            obj.getBoolean("update"),
                            obj.getBoolean("remove"),
                            obj.optString("updateJson"),
                            obj.optBoolean("web"),
                            obj.optBoolean("action")
                        )
                    }.toList()
                isNeedRefresh = false
            }.onFailure { e ->
                Log.e(TAG, "fetchModuleList: ", e)
                isRefreshing = false
            }

            // when both old and new is kotlin.collections.EmptyList
            // moduleList update will don't trigger
            if (oldModuleList === modules) {
                isRefreshing = false
            }

            Log.i(TAG, "load cost: ${SystemClock.elapsedRealtime() - start}, modules: $modules")
        }
    }

    private fun sanitizeVersionString(version: String): String {
        return version.replace(Regex("[^a-zA-Z0-9.\\-_]"), "_")
    }

    fun checkUpdate(m: ModuleInfo): Triple<String, String, String> {
        val empty = Triple("", "", "")
        if (m.updateJson.isEmpty() || m.remove || m.update || !m.enabled) {
            return empty
        }
        // download updateJson
        val result = kotlin.runCatching {
            val url = m.updateJson
            Log.i(TAG, "checkUpdate url: $url")
            val response = ksuApp.okhttpClient.newCall(
                okhttp3.Request.Builder().url(url).build()
            ).execute()
            Log.d(TAG, "checkUpdate code: ${response.code}")
            if (response.isSuccessful) {
                response.body?.string() ?: ""
            } else {
                ""
            }
        }.getOrDefault("")
        Log.i(TAG, "checkUpdate result: $result")

        if (result.isEmpty()) {
            return empty
        }

        val updateJson = kotlin.runCatching {
            JSONObject(result)
        }.getOrNull() ?: return empty

        var version = updateJson.optString("version", "")
        version = sanitizeVersionString(version)
        val versionCode = updateJson.optInt("versionCode", 0)
        val zipUrl = updateJson.optString("zipUrl", "")
        val changelog = updateJson.optString("changelog", "")
        if (versionCode <= m.versionCode || zipUrl.isEmpty()) {
            return empty
        }

        return Triple(zipUrl, version, changelog)
    }
}
