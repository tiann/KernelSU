package me.weishu.kernelsu.ui.util

import android.annotation.SuppressLint
import android.net.Uri
import android.os.Environment
import android.os.Handler
import android.os.Looper
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.util.module.LatestVersionInfo
import okhttp3.Request
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/**
 * @author weishu
 * @date 2023/6/22.
 */
@SuppressLint("Range")
fun download(
    url: String,
    fileName: String,
    onDownloaded: (Uri) -> Unit = {},
    onDownloading: () -> Unit = {},
    onProgress: (Int) -> Unit = {}
) {
    onDownloading()
    Thread {
        val target = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), fileName)
        try {
            ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
                if (!resp.isSuccessful) throw IOException("HTTP ${resp.code}")
                val body = resp.body
                val total = body.contentLength()
                target.parentFile?.mkdirs()
                FileOutputStream(target).use { fos ->
                    val buf = ByteArray(8 * 1024)
                    var read: Int
                    var soFar = 0L
                    val source = body.byteStream()
                    while (true) {
                        read = source.read(buf)
                        if (read == -1) break
                        fos.write(buf, 0, read)
                        soFar += read
                        if (total > 0) {
                            val percent = ((soFar * 100L) / total).toInt().coerceIn(0, 100)
                            onProgress(percent)
                        }
                    }
                    fos.flush()
                }
            }
            Handler(Looper.getMainLooper()).post {
                onDownloaded(Uri.fromFile(target))
            }
        } catch (_: Exception) {
            // ignore, keep UI state
        }
    }.start()
}

fun checkNewVersion(): LatestVersionInfo {
    if (!isNetworkAvailable(ksuApp)) return LatestVersionInfo()
    val url = "https://api.github.com/repos/tiann/KernelSU/releases/latest"
    // default null value if failed
    val defaultValue = LatestVersionInfo()
    runCatching {
        ksuApp.okhttpClient.newCall(Request.Builder().url(url).build()).execute()
            .use { response ->
                if (!response.isSuccessful) {
                    return defaultValue
                }
                val body = response.body.string()
                val json = org.json.JSONObject(body)
                val changelog = json.optString("body")

                val assets = json.getJSONArray("assets")
                for (i in 0 until assets.length()) {
                    val asset = assets.getJSONObject(i)
                    val name = asset.getString("name")
                    if (!name.endsWith(".apk")) {
                        continue
                    }

                    val regex = Regex("v(.+?)_(\\d+)-")
                    val matchResult = regex.find(name) ?: continue
                    matchResult.groupValues[1]
                    val versionCode = matchResult.groupValues[2].toInt()
                    val downloadUrl = asset.getString("browser_download_url")

                    return LatestVersionInfo(
                        versionCode,
                        downloadUrl,
                        changelog
                    )
                }

            }
    }
    return defaultValue
}
