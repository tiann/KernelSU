package me.weishu.kernelsu.ui.util.module

import me.weishu.kernelsu.ksuApp
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject

data class RepoSummary(
    val latestVersion: String,
    val versionCode: Int,
    val downloadUrl: String
)

fun sanitizeVersionString(version: String): String {
    return version.replace(Regex("[^a-zA-Z0-9.\\-_]"), "_")
}

fun fetchRepoIndex(): Map<String, RepoSummary> {
    val url = "https://modules.kernelsu.org/modules.json"
    ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
        if (!resp.isSuccessful) return emptyMap()
        val body = resp.body?.string() ?: return emptyMap()
        val arr = JSONArray(body)
        val result = mutableMapOf<String, RepoSummary>()
        for (idx in 0 until arr.length()) {
            val obj = arr.optJSONObject(idx) ?: continue
            val id = obj.optString("moduleId", "").ifBlank { continue }
            val lr = obj.optJSONObject("latestRelease") ?: continue
            val ver = sanitizeVersionString(lr.optString("name", lr.optString("version", "")))
            val vcode = lr.optInt("versionCode", 0)
            val dl = lr.optString("downloadUrl", "")
            if (vcode > 0 && dl.isNotBlank()) {
                result[id] = RepoSummary(ver, vcode, dl)
            }
        }
        return result
    }
}

data class ModuleDetail(
    val readme: String?,
    val latestTag: String,
    val latestTime: String,
    val latestAssetName: String?,
    val latestAssetUrl: String?,
    val releases: List<ReleaseInfo>
)

data class ReleaseInfo(
    val name: String,
    val tagName: String,
    val publishedAt: String,
    val descriptionHTML: String,
    val assets: List<ReleaseAssetInfo>
)

data class ReleaseAssetInfo(
    val name: String,
    val downloadUrl: String,
    val size: Long
)

fun fetchReleaseDescriptionHtml(moduleId: String, latestTag: String): String? {
    val url = "https://modules.kernelsu.org/module/$moduleId.json"
    ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
        if (!resp.isSuccessful) return null
        val body = resp.body?.string() ?: return null
        val obj = JSONObject(body)
        val releasesArray = obj.optJSONArray("releases") ?: return null
        var fallbackHtml: String? = null
        for (i in 0 until releasesArray.length()) {
            val r = releasesArray.optJSONObject(i) ?: continue
            val descHtml = r.optString("descriptionHTML", "")
            if (fallbackHtml == null && descHtml.isNotBlank()) {
                fallbackHtml = descHtml
            }
            val rname = r.optString("name", r.optString("tagName", r.optString("version", "")))
            if (rname == latestTag && descHtml.isNotBlank()) {
                return descHtml
            }
        }
        return fallbackHtml
    }
}

fun fetchModuleDetail(moduleId: String): ModuleDetail? {
    val url = "https://modules.kernelsu.org/module/$moduleId.json"
    ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
        if (!resp.isSuccessful) return null
        val body = resp.body?.string() ?: return null
        val obj = JSONObject(body)

        val readme = obj.optString("readme", "").ifBlank { null }
    val lr = obj.optJSONObject("latestRelease")
    var latestTag = ""
    var latestTime = ""
    var latestAssetName: String? = null
    var latestAssetUrl: String? = null
    if (lr != null) {
        latestTag = lr.optString("name", lr.optString("version", ""))
        latestTime = lr.optString("time", "")
        val urlDl = lr.optString("downloadUrl", "")
        if (urlDl.isNotEmpty()) {
            latestAssetName = urlDl.substringAfterLast('/')
            latestAssetUrl = urlDl
        }
    } else {
        // fallback: schema may provide latest release as a string
        latestTag = obj.optString("latestRelease", "")
    }

        val releasesArray = obj.optJSONArray("releases")
        val releases = if (releasesArray != null) {
            (0 until releasesArray.length()).mapNotNull { rIdx ->
                val r = releasesArray.optJSONObject(rIdx) ?: return@mapNotNull null
                val rname = r.optString("name", "")
                val publishedAt = r.optString("publishedAt", "")
                val descHtml = r.optString("descriptionHTML", "")
                val assetsArray = r.optJSONArray("releaseAssets") ?: JSONArray()
                val assets = (0 until assetsArray.length()).mapNotNull { aIdx ->
                    val a = assetsArray.optJSONObject(aIdx) ?: return@mapNotNull null
                    val aname = a.optString("name", "")
                    val adl = a.optString("downloadUrl", "")
                    val asz = a.optLong("size", 0L)
                    if (aname.isEmpty() || adl.isEmpty()) null else ReleaseAssetInfo(aname, adl, asz)
                }
                ReleaseInfo(
                    name = rname,
                    tagName = rname,
                    publishedAt = publishedAt,
                    descriptionHTML = descHtml,
                    assets = assets
                )
            }
        } else emptyList()

        return ModuleDetail(
            readme = readme,
            latestTag = latestTag,
            latestTime = latestTime,
            latestAssetName = latestAssetName,
            latestAssetUrl = latestAssetUrl,
            releases = releases
        )
    }
}
