package me.weishu.kernelsu.ui.theme.engine

import java.io.File

/** Display information about an installed theme. */
data class SygiThemeInfo(
    val id: String,
    val name: String,
    val author: String = "",
    val version: String = "",
)

/** One `<text role="..."/>` entry from typography.xml. */
data class SygiTextStyleSpec(
    val font: String? = null,
    val sizeSp: Float? = null,
    val weight: Int? = null,
)

/**
 * A parsed SygiSU theme pack. Packs are pure data: colors, dimensions, shapes, fonts,
 * text styles, icons and extra files. They never contain code.
 */
class SygiThemePack(
    val info: SygiThemeInfo,
    val dir: File,
    private val colors: Map<String, Map<String, Int>>,
    val dimens: Map<String, Float>,
    val integers: Map<String, Int>,
    val corners: Map<String, Float>,
    val fonts: Map<String, File>,
    val textStyles: Map<String, SygiTextStyleSpec>,
) {
    /** Merge colors for the current mode: all -> light, or all -> dark (-> amoled). */
    fun resolveColors(dark: Boolean, amoled: Boolean): Map<String, Int> {
        val out = LinkedHashMap<String, Int>()
        colors["all"]?.let { out.putAll(it) }
        if (dark) {
            colors["dark"]?.let { out.putAll(it) }
            if (amoled) colors["amoled"]?.let { out.putAll(it) }
        } else {
            colors["light"]?.let { out.putAll(it) }
        }
        return out
    }

    /** icons/<id>.png (or webp/jpg), or null when the theme does not override that icon. */
    fun iconFile(id: String): File? {
        if (!SAFE_NAME.matches(id)) return null
        for (ext in ICON_EXTS) {
            val f = File(dir, "icons/$id.$ext")
            if (f.isFile) return f
        }
        return null
    }

    /** A file from any folder in the pack (for example etc/background.png), kept inside the pack directory. */
    fun extraFile(relativePath: String): File? {
        val f = File(dir, relativePath)
        val root = runCatching { dir.canonicalPath }.getOrNull() ?: return null
        val path = runCatching { f.canonicalPath }.getOrNull() ?: return null
        return if (path.startsWith(root + File.separator) && f.isFile) f else null
    }

    companion object {
        val ICON_EXTS = listOf("png", "webp", "jpg", "jpeg")
        private val SAFE_NAME = Regex("[A-Za-z0-9_\\-]+")
    }
}
