package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.LinkAnnotation
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.fromHtml
import androidx.compose.ui.text.style.TextDecoration
import org.commonmark.ext.autolink.AutolinkExtension
import org.commonmark.ext.gfm.strikethrough.StrikethroughExtension
import androidx.compose.ui.graphics.Color
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.commonmark.parser.Parser
import org.commonmark.renderer.html.HtmlRenderer
import ui.feice.mochi.theme.MochiTheme

private val markdownParser by lazy {
    val extensions = listOf(StrikethroughExtension.builder().requireTwoTildes(true).build(), AutolinkExtension.create())
    Parser.builder().extensions(extensions).build() to
        HtmlRenderer.builder().extensions(extensions).sanitizeUrls(true).escapeHtml(true).build()
}

/**
 * Native rich text for changelogs and READMEs. Markdown is converted to HTML and then to AnnotatedString.
 * WebView is not used because it crashes inside the glass layer on Vulkan devices. Images and tables fall back to text.
 */
@Composable
fun MochiRichText(content: String, isMarkdown: Boolean, modifier: Modifier = Modifier) {
    val cs = MochiTheme.colorScheme
    val linkColor = cs.primary
    // May already be parsed by prewarmMochiRichText
    val text = remember(content, isMarkdown, linkColor) { richText(content, isMarkdown, linkColor) }
    val uriHandler = LocalUriHandler.current
    val layout = remember { arrayOfNulls<TextLayoutResult>(1) }
    // No LinkAnnotation: each link gets its own layout node, which made long changelogs slow to animate.
    // Links are plain styled spans and taps are resolved here.
    val tap = if (text.getStringAnnotations(LinkTag, 0, text.length).isEmpty()) {
        Modifier
    } else {
        Modifier.pointerInput(text) {
            detectTapGestures { pos ->
                val l = layout[0] ?: return@detectTapGestures
                val offset = l.getOffsetForPosition(pos)
                text.getStringAnnotations(LinkTag, offset, offset).firstOrNull()?.let { runCatching { uriHandler.openUri(it.item) } }
            }
        }
    }
    // Record once and replay while the dialog animates or scrolls
    BasicText(
        text,
        modifier.graphicsLayer().then(tap),
        style = MochiTheme.textStyles.subhead.copy(color = cs.onSurface),
        onTextLayout = { layout[0] = it },
    )
}

private const val LinkTag = "url"

/** Parses [content] on a background thread and caches it for [MochiRichText]. */
suspend fun prewarmMochiRichText(content: String, isMarkdown: Boolean, linkColor: Color) {
    if (content.isBlank()) return
    withContext(Dispatchers.Default) { richText(content, isMarkdown, linkColor) }
}

private data class RichKey(val content: String, val markdown: Boolean, val link: Color)

private val richCache = object : LinkedHashMap<RichKey, AnnotatedString>(8, 0.75f, true) {
    override fun removeEldestEntry(eldest: MutableMap.MutableEntry<RichKey, AnnotatedString>?) = size > 8
}

private fun richText(content: String, isMarkdown: Boolean, linkColor: Color): AnnotatedString {
    val key = RichKey(content, isMarkdown, linkColor)
    synchronized(richCache) { richCache[key] }?.let { return it }
    val html = if (isMarkdown) {
        val (parser, renderer) = markdownParser
        renderer.render(parser.parse(content))
    } else {
        content
    }
    val text = runCatching { plainLinks(AnnotatedString.fromHtml(html.trim()), linkColor) }.getOrElse { AnnotatedString(content) }
    synchronized(richCache) { richCache[key] = text }
    return text
}

/** Replaces LinkAnnotation with a styled span and a URL string annotation. */
private fun plainLinks(src: AnnotatedString, linkColor: Color): AnnotatedString {
    val links = src.getLinkAnnotations(0, src.length)
    if (links.isEmpty()) return src
    val style = SpanStyle(color = linkColor, textDecoration = TextDecoration.Underline)
    val base = AnnotatedString(
        src.text,
        spanStyles = src.spanStyles + links.map { AnnotatedString.Range(style, it.start, it.end) },
        paragraphStyles = src.paragraphStyles,
    )
    return buildAnnotatedString {
        append(base)
        links.forEach { r -> (r.item as? LinkAnnotation.Url)?.let { addStringAnnotation(LinkTag, it.url, r.start, r.end) } }
    }
}
