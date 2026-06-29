package me.weishu.kernelsu.ui.screen.about

import android.text.Html
import android.text.Spanned
import android.text.style.URLSpan
import androidx.compose.runtime.Immutable

@Immutable
data class LinkInfo(
    val fullText: String,
    val url: String
)

fun extractLinks(html: String): List<LinkInfo> {
    val lines = html.split("<br/>", "<br>", "\n")
    val result = mutableListOf<LinkInfo>()

    for (line in lines) {
        val spanned: Spanned = Html.fromHtml(line, Html.FROM_HTML_MODE_LEGACY)
        val spans = spanned.getSpans(0, spanned.length, URLSpan::class.java)
        val text = spanned.toString().trim()

        for (span in spans) {
            val url = span.url
            result.add(LinkInfo(text, url))
        }
    }
    return result
}

