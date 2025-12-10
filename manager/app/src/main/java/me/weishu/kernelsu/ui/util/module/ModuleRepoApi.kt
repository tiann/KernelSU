package me.weishu.kernelsu.ui.util.module

import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.util.isNetworkAvailable
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject

data class ModuleDetail(
    val readme: String,
    val readmeHtml: String,
    val latestTag: String,
    val latestTime: String,
    val latestAssetName: String?,
    val latestAssetUrl: String?,
    val releases: List<ReleaseInfo>,
    val homepageUrl: String,
    val sourceUrl: String,
    val url: String
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
    val size: Long,
    val downloadCount: Int
)

fun sanitizeVersionString(version: String): String {
    return version.replace(Regex("[^a-zA-Z0-9.\\-_]"), "_")
}

fun stripTicks(s: String): String {
    val t = s.trim()
    return if (t.startsWith("`") && t.endsWith("`") && t.length >= 2) t.substring(1, t.length - 1) else t
}

fun fetchReleaseDescriptionHtml(moduleId: String, latestTag: String): String? {
    if (!isNetworkAvailable(ksuApp)) return null
    val url = "https://modules.kernelsu.org/module/$moduleId.json"
    return runCatching {
        ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
            if (!resp.isSuccessful) null else {
                val body = resp.body?.string() ?: return@use null
                val obj = JSONObject(body)
                val releasesArray = obj.optJSONArray("releases") ?: return@use null
                var fallbackHtml: String? = null
                for (i in 0 until releasesArray.length()) {
                    val r = releasesArray.optJSONObject(i) ?: continue
                    val descHtml = r.optString("descriptionHTML", "")
                    if (fallbackHtml == null && descHtml.isNotBlank()) {
                        fallbackHtml = descHtml
                    }
                    val rname = r.optString("name", r.optString("tagName", r.optString("version", "")))
                    if (rname == latestTag && descHtml.isNotBlank()) {
                        return@use descHtml
                    }
                }
                fallbackHtml
            }
        }
    }.getOrNull()
}


fun fetchModuleDetail(moduleId: String): ModuleDetail? {
    if (!isNetworkAvailable(ksuApp)) return null
    val url = "https://modules.kernelsu.org/module/$moduleId.json"
    return runCatching {
        ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
            if (!resp.isSuccessful) return@use null
            val body = resp.body?.string() ?: return@use null
            val obj = JSONObject(body)
            val readme = obj.optString("readme", "")
            val readmeHtml = obj.optString("readmeHTML", "")
            val homepageUrl = stripTicks(obj.optString("homepageUrl", ""))
            val sourceUrl = stripTicks(obj.optString("sourceUrl", ""))
            val url = stripTicks(obj.optString("url", ""))
            val lr = obj.optJSONObject("latestRelease")
            var latestTag: String
            var latestTime = ""
            var latestAssetName: String? = null
            var latestAssetUrl: String? = null
            if (lr != null) {
                latestTag = lr.optString("name", lr.optString("version", ""))
                latestTime = lr.optString("time", "")
                var urlDl = lr.optString("downloadUrl", "")
                urlDl = stripTicks(urlDl)
                if (urlDl.isNotEmpty()) {
                    latestAssetName = urlDl.substringAfterLast('/')
                    latestAssetUrl = urlDl
                }
            } else {
                latestTag = obj.optString("latestRelease", "")
            }

            val releasesArray = obj.optJSONArray("releases")
            val releases = if (releasesArray != null) {
                (0 until releasesArray.length()).mapNotNull { rIdx ->
                    val r = releasesArray.optJSONObject(rIdx) ?: return@mapNotNull null
                    val rname = r.optString("name", r.optString("tagName", r.optString("version", "")))
                    val publishedAt = r.optString("publishedAt", "")
                    val descHtml = r.optString("descriptionHTML", "")
                    val assetsArray = r.optJSONArray("releaseAssets") ?: JSONArray()
                    val assets = (0 until assetsArray.length()).mapNotNull { aIdx ->
                        val a = assetsArray.optJSONObject(aIdx) ?: return@mapNotNull null
                        val aname = a.optString("name", "")
                        var adl = a.optString("downloadUrl", "")
                        adl = stripTicks(adl)
                        val asz = a.optLong("size", 0L)
                        val dcnt = when (val dcAny = a.opt("downloadCount")) {
                            is Number -> dcAny.toInt()
                            is String -> dcAny.toIntOrNull() ?: 0
                            else -> 0
                        }
                        if (aname.isEmpty() || adl.isEmpty()) null else ReleaseAssetInfo(aname, adl, asz, dcnt)
                    }
                    ReleaseInfo(
                        name = rname,
                        tagName = r.optString("tagName", rname),
                        publishedAt = publishedAt,
                        descriptionHTML = descHtml,
                        assets = assets
                    )
                }
            } else emptyList()

            return@use ModuleDetail(
                readme = readme,
                readmeHtml = readmeHtml,
                latestTag = latestTag,
                latestTime = latestTime,
                latestAssetName = latestAssetName,
                latestAssetUrl = latestAssetUrl,
                releases = releases,
                homepageUrl = homepageUrl,
                sourceUrl = sourceUrl,
                url = url
            )
        }
    }.getOrNull()
}
