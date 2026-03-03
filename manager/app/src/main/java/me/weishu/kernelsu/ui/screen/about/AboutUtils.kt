package me.weishu.kernelsu.ui.screen.about

import android.util.Log

data class LinkInfo(
    val fullText: String,
    val url: String
)

fun extractLinks(html: String): List<LinkInfo> {
    val regex = Regex(
        """([^<>\n\r]+?)\s*<b>\s*<a\b[^>]*\bhref\s*=\s*(['"]?)([^'"\s>]+)\2[^>]*>([^<]+)</a>\s*</b>\s*(.*?)\s*(?=<br|\n|$)""",
        RegexOption.MULTILINE
    )

    return regex.findAll(html).mapNotNull { match ->
        try {
            val before = match.groupValues[1].trim()
            val url = match.groupValues[3].trim()
            val title = match.groupValues[4].trim()
            val after = match.groupValues[5].trim()

            val fullText = "$before $title $after"
            LinkInfo(fullText, url)
        } catch (e: Exception) {
            Log.e("AboutState", "extractLinks failed: ${e.message}")
            null
        }
    }.toList()
}
