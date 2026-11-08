package me.weishu.kernelsu.ui.theme.engine

import android.graphics.Color as AndroidColor
import android.util.Xml
import org.xmlpull.v1.XmlPullParser
import java.io.File

/**
 * Reads a theme folder (already extracted and validated by [SygiThemeImporter]).
 *
 * XML files inside theme/ use Android's resources style:
 *   <color name="primary">#RRGGBB</color>      (colors_light.xml, colors_dark.xml, colors_amoled.xml, colors_any.xml)
 *   <dimen name="nav_corner">32dp</dimen>
 *   <integer name="icons_use_original_colors">1</integer>
 *   <corner name="large" dp="24"/>
 *   <font name="default" file="fonts/MyFont.ttf"/>
 *   <text role="titleLarge" font="default" size="22" weight="600"/>
 * A "_light", "_dark" or "_amoled" file name suffix limits a file to that mode.
 */
object SygiThemeParser {
    private val FILE_RE = Regex("^(.*?)(?:_(light|dark|amoled))?\\.xml$", RegexOption.IGNORE_CASE)

    fun readInfo(dir: File, fallbackId: String = dir.name): SygiThemeInfo {
        var name = fallbackId
        var author = ""
        var version = ""
        val manifest = File(dir, "theme.xml")
        if (manifest.isFile) {
            forEachElement(manifest) { tag, attrs, _ ->
                if (tag == "theme") {
                    attrs["name"]?.takeIf { it.isNotBlank() }?.let { name = it.take(64) }
                    author = attrs["author"].orEmpty().take(64)
                    version = attrs["version"].orEmpty().take(32)
                }
            }
        }
        return SygiThemeInfo(id = fallbackId, name = name, author = author, version = version)
    }

    fun parse(dir: File, fallbackId: String = dir.name): SygiThemePack {
        val info = readInfo(dir, fallbackId)
        val colors = HashMap<String, MutableMap<String, Int>>()
        val dimens = HashMap<String, Float>()
        val integers = HashMap<String, Int>()
        val corners = HashMap<String, Float>()
        val fonts = HashMap<String, File>()
        val textStyles = HashMap<String, SygiTextStyleSpec>()

        val files = File(dir, "theme").listFiles { f -> f.isFile && f.name.endsWith(".xml", ignoreCase = true) }
            ?.sortedBy { it.name }
            .orEmpty()

        for (file in files) {
            val variant = FILE_RE.matchEntire(file.name)?.groupValues?.getOrNull(2)
                ?.lowercase()
                ?.ifEmpty { "all" } ?: "all"
            forEachElement(file) { tag, attrs, text ->
                val name = attrs["name"].orEmpty()
                when (tag) {
                    "color" -> parseColor(text)?.let { c ->
                        if (name.isNotEmpty()) colors.getOrPut(variant) { HashMap() }[name] = c
                    }

                    "dimen" -> text.removeSuffix("dp").removeSuffix("sp").trim().toFloatOrNull()?.let {
                        if (name.isNotEmpty()) dimens[name] = it.coerceIn(0f, 512f)
                    }

                    "integer" -> text.trim().toIntOrNull()?.let {
                        if (name.isNotEmpty()) integers[name] = it
                    }

                    "corner" -> (attrs["dp"] ?: text).removeSuffix("dp").trim().toFloatOrNull()?.let {
                        if (name.isNotEmpty()) corners[name] = it.coerceIn(0f, 128f)
                    }

                    "font" -> {
                        val rel = attrs["file"].orEmpty()
                        val f = File(dir, rel)
                        if (name.isNotEmpty() && isInside(File(dir, "fonts"), f) && f.isFile) fonts[name] = f
                    }

                    "text" -> {
                        val role = attrs["role"].orEmpty()
                        if (role.isNotEmpty()) {
                            textStyles[role] = SygiTextStyleSpec(
                                font = attrs["font"],
                                sizeSp = attrs["size"]?.removeSuffix("sp")?.trim()?.toFloatOrNull()?.coerceIn(6f, 96f),
                                weight = attrs["weight"]?.trim()?.toIntOrNull(),
                            )
                        }
                    }
                }
            }
        }
        return SygiThemePack(info, dir, colors, dimens, integers, corners, fonts, textStyles)
    }

    private fun parseColor(text: String): Int? =
        runCatching { AndroidColor.parseColor(text.trim()) }.getOrNull()

    private fun isInside(root: File, file: File): Boolean {
        val r = runCatching { root.canonicalPath }.getOrNull() ?: return false
        val p = runCatching { file.canonicalPath }.getOrNull() ?: return false
        return p.startsWith(r + File.separator)
    }

    /** Visits every element except <resources>. A malformed file is skipped, never fatal. */
    private fun forEachElement(file: File, block: (tag: String, attrs: Map<String, String>, text: String) -> Unit) {
        runCatching {
            file.inputStream().use { input ->
                val parser = Xml.newPullParser()
                parser.setInput(input, "UTF-8")
                var event = parser.eventType
                while (event != XmlPullParser.END_DOCUMENT) {
                    if (event == XmlPullParser.START_TAG && parser.name != "resources") {
                        val attrs = HashMap<String, String>()
                        for (i in 0 until parser.attributeCount) {
                            attrs[parser.getAttributeName(i)] = parser.getAttributeValue(i)
                        }
                        val tag = parser.name
                        val text = parser.nextText().trim()
                        block(tag, attrs, text)
                    }
                    event = parser.next()
                }
            }
        }
    }
}
