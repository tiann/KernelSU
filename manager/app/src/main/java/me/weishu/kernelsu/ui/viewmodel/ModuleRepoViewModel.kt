package me.weishu.kernelsu.ui.viewmodel

import android.util.Log
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.util.HanziToPinyin
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject

class ModuleRepoViewModel : ViewModel() {

    companion object {
        private const val TAG = "ModuleRepoViewModel"
        private const val MODULES_URL = "https://modules.kernelsu.org/modules.json"
    }

    @Immutable
    data class Author(
        val name: String,
        val link: String,
    )

    @Immutable
    data class ReleaseAsset(
        val name: String,
        val downloadUrl: String,
        val size: Long
    )

    @Immutable
    data class Release(
        val tagName: String,
        val name: String,
        val publishedAt: String,
        val assets: List<ReleaseAsset>
    )

    @Immutable
    data class RepoModule(
        val moduleId: String,
        val moduleName: String,
        val authors: String,
        val authorList: List<Author>,
        val summary: String,
        val homepageUrl: String,
        val sourceUrl: String,
        val latestRelease: String,
        val latestReleaseTime: String,
        val releases: List<Release>,
    )

    private var _modules = mutableStateOf<List<RepoModule>>(emptyList())
    val modules: State<List<RepoModule>> = _modules

    var isRefreshing by mutableStateOf(false)
        private set

    private val _searchStatus = mutableStateOf(SearchStatus(""))
    val searchStatus: State<SearchStatus> = _searchStatus

    private val _searchResults = mutableStateOf<List<RepoModule>>(emptyList())
    val searchResults: State<List<RepoModule>> = _searchResults

    fun refresh() {
        viewModelScope.launch {
            withContext(Dispatchers.Main) { isRefreshing = true }
            val parsed = withContext(Dispatchers.IO) { fetchModulesInternal() }
            withContext(Dispatchers.Main) {
                _modules.value = parsed
                isRefreshing = false
            }
        }
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
            _modules.value.filter {
                it.moduleId.contains(text, true)
                        || it.moduleName.contains(text, true)
                        || it.authors.contains(text, true)
                        || it.summary.contains(text, true)
                        || HanziToPinyin.getInstance().toPinyinString(it.moduleName).contains(text, true)
            }
        }

        _searchResults.value = result
        _searchStatus.value.resultStatus = if (result.isEmpty()) {
            SearchStatus.ResultStatus.EMPTY
        } else {
            SearchStatus.ResultStatus.SHOW
        }
    }

    private fun fetchModulesInternal(): List<RepoModule> {
        return runCatching {
            val request = Request.Builder().url(MODULES_URL).build()
            ksuApp.okhttpClient.newCall(request).execute().use { resp ->
                if (!resp.isSuccessful) return emptyList()
                val body = resp.body?.string() ?: return emptyList()
                val json = kotlin.runCatching { JSONArray(body) }.getOrElse {
                    val obj = JSONObject(body)
                    obj.optJSONArray("modules") ?: JSONArray()
                }
                (0 until json.length()).mapNotNull { idx ->
                    val item = json.optJSONObject(idx) ?: return@mapNotNull null
                    parseRepoModule(item)
                }
            }
        }.getOrElse {
            Log.e(TAG, "fetch modules failed", it)
            emptyList()
        }
    }

    private fun parseRepoModule(item: JSONObject): RepoModule? {
        val moduleId = item.optString("moduleId", "")
        if (moduleId.isEmpty()) return null
        val moduleName = item.optString("moduleName", "")
        val authorsArray = item.optJSONArray("authors")
        val authorList = if (authorsArray != null) {
            (0 until authorsArray.length())
                .mapNotNull { idx ->
                    val authorObj = authorsArray.optJSONObject(idx) ?: return@mapNotNull null
                    val name = authorObj.optString("name", "").trim()
                    val link = authorObj.optString("link", "").trim()
                    if (name.isEmpty()) null else Author(name = name, link = link)
                }
        } else {
            emptyList()
        }
        val authors = if (authorList.isNotEmpty()) authorList.joinToString(", ") { it.name } else item.optString("authors", "")
        val summary = item.optString("summary", "")
        val homepageUrl = item.optString("homepageUrl", item.optString("url", ""))
        val sourceUrl = item.optString("sourceUrl", item.optString("url", ""))
        val latestRelease = item.optString("latestRelease", "")
        val latestReleaseTime = item.optString("latestReleaseTime", "")

        val releasesArray = item.optJSONArray("releases") ?: JSONArray()
        val releases = (0 until releasesArray.length()).mapNotNull { rIdx ->
            val r = releasesArray.optJSONObject(rIdx) ?: return@mapNotNull null
            val tag = r.optString("tagName", r.optString("name", ""))
            val rname = r.optString("name", tag)
            val publishedAt = r.optString("publishedAt", r.optString("updatedAt", ""))
            val assetsArray = r.optJSONArray("releaseAssets") ?: r.optJSONArray("assets") ?: JSONArray()
            val assets = (0 until assetsArray.length()).mapNotNull { aIdx ->
                val a = assetsArray.optJSONObject(aIdx) ?: return@mapNotNull null
                val aname = a.optString("name", "")
                val downloadUrl = a.optString("downloadUrl", a.optString("browser_download_url", ""))
                if (aname.isEmpty() || downloadUrl.isEmpty()) null else ReleaseAsset(
                    name = aname,
                    downloadUrl = downloadUrl,
                    size = a.optLong("size", 0L)
                )
            }
            Release(tagName = tag, name = rname, publishedAt = publishedAt, assets = assets)
        }

        return RepoModule(
            moduleId = moduleId,
            moduleName = moduleName,
            authors = authors,
            authorList = authorList,
            summary = summary,
            homepageUrl = homepageUrl,
            sourceUrl = sourceUrl,
            latestRelease = latestRelease,
            latestReleaseTime = latestReleaseTime,
            releases = releases,
        )
    }
}
