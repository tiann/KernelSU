package me.weishu.kernelsu.ui.theme.engine

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.util.zip.ZipInputStream

/**
 * Imports a theme .zip safely. Themes are untrusted input, so the importer:
 *  - rejects path tricks (absolute paths, "..", drive letters) and checks canonical paths (zip-slip)
 *  - allows only data file types (images, fonts, xml, text); anything else rejects the whole theme
 *  - caps entry count and sizes
 *  - rejects XML containing DOCTYPE / ENTITY declarations
 * Both "/" and "\" are accepted as path separators in the zip.
 */
object SygiThemeImporter {
    private const val MAX_ENTRIES = 600
    private const val MAX_ENTRY_BYTES = 20L * 1024 * 1024
    private const val MAX_TOTAL_BYTES = 120L * 1024 * 1024
    private val ALLOWED_EXT = setOf("png", "webp", "jpg", "jpeg", "xml", "ttf", "otf", "txt", "md")
    private val IGNORED_NAMES = setOf(".DS_Store", "Thumbs.db")

    class ImportException(message: String) : Exception(message)

    fun importZip(context: Context, uri: Uri): SygiThemeInfo {
        val base = SygiThemeManager.themesDir.apply { mkdirs() }
        val tmp = File(base, ".tmp-${System.nanoTime()}")
        try {
            tmp.mkdirs()
            extract(context, uri, tmp)

            var root = tmp
            val kids = tmp.listFiles().orEmpty()
            if (!File(tmp, "theme").isDirectory && kids.size == 1 && kids[0].isDirectory) root = kids[0]

            val themeXml = File(root, "theme").listFiles { f -> f.isFile && f.name.endsWith(".xml", true) }.orEmpty()
            if (themeXml.isEmpty()) throw ImportException("Theme has no theme/ folder with XML files")

            val displayName = queryName(context, uri).removeSuffix(".zip").removeSuffix(".ZIP")
            val parsed = SygiThemeParser.readInfo(root, fallbackId = displayName)
            val id = sanitizeId(if (parsed.name.isNotBlank()) parsed.name else displayName)

            val target = File(base, id)
            if (target.exists()) target.deleteRecursively()
            if (!root.renameTo(target)) throw ImportException("Could not store theme")
            return parsed.copy(id = id)
        } finally {
            tmp.deleteRecursively()
        }
    }

    private fun extract(context: Context, uri: Uri, tmp: File) {
        val tmpCanon = tmp.canonicalPath
        var entries = 0
        var total = 0L
        val input = context.contentResolver.openInputStream(uri) ?: throw ImportException("Could not open file")
        input.use { raw ->
            ZipInputStream(raw.buffered()).use { zis ->
                var entry = zis.nextEntry
                while (entry != null) {
                    if (++entries > MAX_ENTRIES) throw ImportException("Too many files in theme")
                    val original = entry.name
                    val name = original.replace('\\', '/')
                    val fileName = name.substringAfterLast('/')
                    val skip = entry.isDirectory || name.startsWith("__MACOSX/") || fileName in IGNORED_NAMES
                    if (!skip) {
                        if (name.startsWith("/") || name.contains(':') || name.split('/').any { it == ".." }) {
                            throw ImportException("Unsafe path in zip: $original")
                        }
                        val ext = fileName.substringAfterLast('.', "").lowercase()
                        if (ext !in ALLOWED_EXT) throw ImportException("File type not allowed: $original")
                        val out = File(tmp, name)
                        if (!out.canonicalPath.startsWith(tmpCanon + File.separator)) {
                            throw ImportException("Unsafe path in zip: $original")
                        }
                        out.parentFile?.mkdirs()
                        var size = 0L
                        out.outputStream().use { os ->
                            val buf = ByteArray(8192)
                            while (true) {
                                val n = zis.read(buf)
                                if (n < 0) break
                                size += n
                                total += n
                                if (size > MAX_ENTRY_BYTES || total > MAX_TOTAL_BYTES) throw ImportException("Theme is too large")
                                os.write(buf, 0, n)
                            }
                        }
                        if (ext == "xml") {
                            val text = out.readText()
                            if (text.contains("<!DOCTYPE", true) || text.contains("<!ENTITY", true)) {
                                throw ImportException("XML with DOCTYPE or ENTITY is not allowed: $original")
                            }
                        }
                    }
                    zis.closeEntry()
                    entry = zis.nextEntry
                }
            }
        }
    }

    private fun sanitizeId(name: String): String {
        val id = name.lowercase().replace(Regex("[^a-z0-9_-]"), "_").trim('_').take(48)
        return id.ifBlank { "theme" }
    }

    private fun queryName(context: Context, uri: Uri): String {
        val fromProvider = runCatching {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0) else null
            }
        }.getOrNull()
        return fromProvider ?: uri.lastPathSegment ?: "theme"
    }
}
