package me.weishu.kernelsu.ui.webui.webview.asset

import android.content.Context
import android.util.Log
import android.webkit.WebResourceResponse
import androidx.annotation.WorkerThread
import androidx.webkit.WebViewAssetLoader.PathHandler
import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.io.SuFile
import com.topjohnwu.superuser.io.SuFileInputStream
import me.weishu.kernelsu.ui.webui.util.MimeUtil
import me.weishu.kernelsu.ui.webui.webview.asset.MonetColorsProvider.getColorsCss
import java.io.ByteArrayInputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.nio.charset.StandardCharsets
import java.util.zip.GZIPInputStream

private const val DEFAULT_MIME_TYPE = "text/plain"
private const val TAG = "SuFilePathHandler"
private val FORBIDDEN_DATA_DIRS = arrayOf("/data/data", "/data/system")

class SuFilePathHandler(
    private val context: Context,
    webRoot: File,
    private val shell: Shell,
    private val onInsetsRequested: (Boolean) -> Unit,
) : PathHandler {
    private val directory: File = runCatching { File(getCanonicalDirPath(webRoot)) }
        .getOrElse {
            throw IllegalArgumentException(
                "Failed to resolve the canonical path for the given directory: ${webRoot.path}", it
            )
        }

    init {
        require(isAllowedInternalStorageDir()) {
            "The given directory \"$webRoot\" doesn't exist under an allowed app internal storage directory"
        }
    }

    @WorkerThread
    override fun handle(path: String): WebResourceResponse {
        if (path == "internal/insets.css") {
            onInsetsRequested(true)
            val css = """
                :root {
                  --safe-area-inset-top: env(safe-area-inset-top, 0px);
                  --safe-area-inset-right: env(safe-area-inset-right, 0px);
                  --safe-area-inset-bottom: env(safe-area-inset-bottom, 0px);
                  --safe-area-inset-left: env(safe-area-inset-left, 0px);
                  --window-inset-top: var(--safe-area-inset-top);
                  --window-inset-bottom: var(--safe-area-inset-bottom);
                  --window-inset-left: var(--safe-area-inset-left);
                  --window-inset-right: var(--safe-area-inset-right);
                  --f7-safe-area-top: var(--window-inset-top) !important;
                  --f7-safe-area-bottom: var(--window-inset-bottom) !important;
                  --f7-safe-area-left: var(--window-inset-left) !important;
                  --f7-safe-area-right: var(--window-inset-right) !important;
                }
            """.trimIndent()
            return cssResponse(css)
        }
        if (path == "internal/colors.css") {
            val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
            val colorMode = prefs.getInt("color_mode", 0)
            val uiMode = prefs.getString("ui_mode", "miuix") ?: "miuix"
            // Color variables are only produced for the monet/custom color modes (3..6) or the
            // Material UI, so anything else must not receive them.
            val css = if (colorMode in 3..6 || uiMode == "material") getColorsCss() else ""
            return cssResponse(css)
        }

        try {
            val file = getCanonicalFileIfChild(directory, path)
            if (file == null) {
                Log.e(
                    TAG,
                    String.format(
                        "The requested file: %s is outside the mounted directory: %s",
                        path,
                        directory
                    )
                )
                return WebResourceResponse(null, null, null)
            }
            val stream = openFile(file, shell)
            return WebResourceResponse(guessMimeType(path), null, stream)
        } catch (e: IOException) {
            Log.e(TAG, "Error opening the requested path: $path", e)
            return WebResourceResponse(null, null, null)
        }
    }

    private fun cssResponse(css: String) =
        WebResourceResponse(
            "text/css", "utf-8", ByteArrayInputStream(css.toByteArray(StandardCharsets.UTF_8))
        )

    private fun isAllowedInternalStorageDir(): Boolean {
        val dir = getCanonicalDirPath(directory)
        for (forbiddenPath in FORBIDDEN_DATA_DIRS) {
            if (dir.startsWith(forbiddenPath)) return false
        }
        return true
    }

    private fun getCanonicalDirPath(file: File): String {
        var canonicalPath = file.canonicalPath
        if (!canonicalPath.endsWith("/")) canonicalPath += "/"
        return canonicalPath
    }

    private fun getCanonicalFileIfChild(parent: File, child: String): File? {
        val parentCanonicalPath = getCanonicalDirPath(parent)
        val childCanonicalPath = File(parent, child).canonicalPath
        return if (childCanonicalPath.startsWith(parentCanonicalPath)) File(childCanonicalPath) else null
    }

    private fun openFile(file: File, shell: Shell): InputStream {
        val suFile = SuFile(file.absolutePath)
        suFile.shell = shell
        val fis = SuFileInputStream.open(suFile)
        return if (file.path.endsWith(".svgz")) GZIPInputStream(fis) else fis
    }

    private fun guessMimeType(filePath: String): String =
        MimeUtil.getMimeFromFileName(filePath) ?: DEFAULT_MIME_TYPE

}
