package me.weishu.kernelsu.ui.component.markdown

import android.annotation.SuppressLint
import android.content.ActivityNotFoundException
import android.content.Intent
import android.graphics.Color
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.webkit.JavascriptInterface
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.viewinterop.AndroidView
import androidx.webkit.WebViewAssetLoader
import me.weishu.kernelsu.data.registry.RegistryUrls
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.adjustLightnessArgb
import me.weishu.kernelsu.ui.util.cssColorFromArgb
import me.weishu.kernelsu.ui.util.ensureVisibleByMix
import me.weishu.kernelsu.ui.util.relativeLuminance
import okhttp3.Headers.Companion.toHeaders
import okhttp3.OkHttpClient
import okhttp3.Request
import okio.IOException
import org.commonmark.ext.autolink.AutolinkExtension
import org.commonmark.ext.gfm.strikethrough.StrikethroughExtension
import org.commonmark.ext.gfm.tables.TablesExtension
import org.commonmark.ext.task.list.items.TaskListItemsExtension
import org.commonmark.parser.Parser
import org.commonmark.renderer.html.HtmlRenderer
import top.yukonga.miuix.kmp.theme.MiuixTheme
import java.io.ByteArrayInputStream
import java.nio.charset.StandardCharsets
import kotlin.math.abs

private const val APP_ASSETS_ORIGIN = "https://appassets.androidplatform.net"
private val SEMICOLON_SPLIT = ";\\s*".toRegex()
private val EQUALS_SPLIT = "=\\s*".toRegex()

@SuppressLint("JavascriptInterface", "SetJavaScriptEnabled", "WrongConstant")
@Composable
fun GithubMarkdown(
    content: String,
    isMarkdown: Boolean = false,
    onLoadingChange: (Boolean) -> Unit = {},
    containerColor: androidx.compose.ui.graphics.Color? = null,
    /**
     * Site root of untrusted, already sanitized remote HTML (e.g. KernelSU Modules readmes and
     * changelogs). When set, the WebView runs locked down: JavaScript and DOM storage are off,
     * no JS bridge, no file/content access, no mixed content, sub-resources only from this
     * site's host (plus bundled assets), relative URLs resolve against it, raw HTML in Markdown
     * is escaped, nothing is ever loaded in place, and only web links the user tapped are
     * handed to an external browser.
     */
    restrictedBaseUrl: String? = null,
    modifier: Modifier = Modifier,
) {
    val restricted = restrictedBaseUrl != null
    val restrictedClient = remember(restrictedBaseUrl) { restrictedBaseUrl?.let(::restrictedResourceClient) }
    val density = LocalDensity.current
    val systemDensity = LocalResources.current.displayMetrics.density
    val fontScale = density.fontScale
    val pageScale = density.density / systemDensity
    val newTextZoom = (90 * pageScale * fontScale).toInt()
    val scrollInterface = remember { MarkdownScrollInterface() }

    val isDark = isInDarkTheme()
    val dir = if (LocalLayoutDirection.current == LayoutDirection.Rtl) "rtl" else "ltr"

    val colors = getMarkdownColors(containerColor)
    val bgCode = colors.bgCode
    val bgRowAlt = colors.bgRowAlt
    val fgDefault = colors.fgDefault
    val fgLink = colors.fgLink

    val template = remember(isDark) {
        val name = if (isDark) "webview/template_dark.html" else "webview/template.html"
        ksuApp.assets.open(name).bufferedReader(StandardCharsets.UTF_8).use { it.readText() }
    }
    val extensions = remember {
        listOf(
            TablesExtension.create(),
            StrikethroughExtension.builder().requireTwoTildes(true).build(),
            AutolinkExtension.create(),
            TaskListItemsExtension.create(),
        )
    }
    val parser = remember(extensions) { Parser.builder().extensions(extensions).build() }
    val renderer = remember(extensions, restricted) {
        HtmlRenderer.builder().extensions(extensions).sanitizeUrls(restricted).escapeHtml(restricted).build()
    }
    val rendered = remember(content, isMarkdown) {
        if (isMarkdown) renderer.render(parser.parse(content)) else content
    }
    val styleContent = """
        :root {
            --background: ${Color.TRANSPARENT};
            --pre-background: $bgCode;
            --code-background: $bgCode;
            --tr-alt-background: $bgRowAlt;
            --thead-background: $bgRowAlt;
            --textPrimary: $fgDefault;
            --link: $fgLink;
        }
        html, body { margin: 0; padding: 0 }
        img, video { max-width: 100%; height: auto; }
        .markdown-body { padding: 16px; }
        .markdown-body table { display: block; max-width: 100%; overflow-x: auto; }
        .markdown-body pre { overflow-x: auto; }
    """.trimIndent()
    val html = template
        .let { t ->
            // The document URL is the registry site root (relative links resolve against it and
            // `#anchor` links stay in the same document); bundled stylesheets keep loading from
            // the asset loader.
            if (restrictedBaseUrl == null) t else t
                .replace("href=\"/assets/webview/", "href=\"$APP_ASSETS_ORIGIN/assets/webview/")
                .replace(Regex("<script[^>]*>.*?</script>", RegexOption.DOT_MATCHES_ALL), "")
        }
        .replace("@dir@", dir)
        .replace("@style@", styleContent)
        .replace("@body@", rendered)

    AndroidView(
        factory = { context ->
            val frameLayout = FrameLayout(context)
            val webView = EdgeAwareWebView(context).apply {
                try {
                    setBackgroundColor(Color.TRANSPARENT)
                    isVerticalScrollBarEnabled = false
                    isHorizontalScrollBarEnabled = false

                    settings.apply {
                        offscreenPreRaster = true
                        javaScriptEnabled = !restricted
                        domStorageEnabled = !restricted
                        mixedContentMode = if (restricted) {
                            WebSettings.MIXED_CONTENT_NEVER_ALLOW
                        } else {
                            WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
                        }
                        allowContentAccess = false
                        allowFileAccess = false
                        javaScriptCanOpenWindowsAutomatically = false
                        setSupportMultipleWindows(false)
                        cacheMode = WebSettings.LOAD_CACHE_ELSE_NETWORK
                        textZoom = newTextZoom
                        setSupportZoom(false)
                        setGeolocationEnabled(false)
                    }
                    layoutParams = FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT
                    )
                    if (!restricted) {
                        addJavascriptInterface(scrollInterface, "AndroidScroll")
                    }
                    webViewClient = object : WebViewClient() {
                        private val assetLoader = WebViewAssetLoader.Builder()
                            .addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(context))
                            .build()

                        override fun onPageFinished(view: WebView, url: String) {
                            super.onPageFinished(view, url)
                            if (restricted) return

                            val js = """
                                (function() {
                                    if (window.androidScrollInjected) return;
                                    window.androidScrollInjected = true;
                                
                                    function checkScroll(target) {
                                        if (!target || target === document.body || target === document.documentElement) return {l: false, r: false};
                                        var style = window.getComputedStyle(target);
                                        if (style.overflowX !== 'auto' && style.overflowX !== 'scroll') return {l: false, r: false};
                                        if (target.scrollWidth <= target.clientWidth) return {l: false, r: false};
                                        
                                        var atLeft = target.scrollLeft <= 0;
                                        var atRight = Math.ceil(target.scrollLeft + target.clientWidth) >= target.scrollWidth;
                                        
                                        return {l: !atLeft, r: !atRight};
                                    }
                                
                                    var lastTarget = null;
                                    var lastState = {l: false, r: false};
                                    
                                    function update(l, r) {
                                        if (lastState.l !== l || lastState.r !== r) {
                                            lastState = {l: l, r: r};
                                            AndroidScroll.updateScrollState(l, r);
                                        }
                                    }
                                
                                    document.addEventListener('touchstart', function(e) {
                                        var t = e.target;
                                        var found = false;
                                        while(t && t !== document.body) {
                                            var s = checkScroll(t);
                                            if (s.l || s.r) { 
                                                 lastTarget = t;
                                                 update(s.l, s.r);
                                                 found = true;
                                                 break;
                                            }
                                            t = t.parentElement;
                                        }
                                        if (!found) {
                                            lastTarget = null;
                                            update(false, false);
                                        }
                                    }, {passive: true});
                                
                                    document.addEventListener('touchmove', function(e) {
                                        if (lastTarget) {
                                             var s = checkScroll(lastTarget);
                                             update(s.l, s.r);
                                        }
                                    }, {passive: true});
                                    
                                    document.addEventListener('scroll', function(e) {
                                        if (lastTarget && (e.target === lastTarget || e.target.contains(lastTarget))) {
                                              var s = checkScroll(lastTarget);
                                              update(s.l, s.r);
                                        }
                                    }, {passive: true, capture: true});
                                })();
                            """.trimIndent()
                            view.evaluateJavascript(js, null)
                        }

                        override fun shouldOverrideUrlLoading(
                            view: WebView, request: WebResourceRequest
                        ): Boolean {
                            if (restrictedBaseUrl != null) {
                                // `#anchor` of this document: the view is as tall as its content,
                                // so there is nothing to scroll to; never leave the app for it.
                                if (RegistryUrls.isInPageAnchor(request.url.toString(), restrictedBaseUrl)) return true
                                // Never navigate inside the view; only web links the user tapped
                                // leave the app (no automatic redirects such as <meta refresh>).
                                if (!request.hasGesture()) return true
                                val scheme = request.url.scheme?.lowercase()
                                if (scheme != "https" && scheme != "http") return true
                            }
                            try {
                                val intent = Intent(Intent.ACTION_VIEW, request.url)
                                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                                context.startActivity(intent)
                            } catch (_: ActivityNotFoundException) {
                                Log.w("GithubMarkdown", "No activity to handle: ${request.url}")
                            }
                            return true
                        }

                        override fun onPageCommitVisible(view: WebView?, url: String?) {
                            onLoadingChange(false)
                        }

                        override fun shouldInterceptRequest(
                            view: WebView, request: WebResourceRequest
                        ): WebResourceResponse? {
                            assetLoader.shouldInterceptRequest(request.url)?.let { return it }
                            if (restrictedBaseUrl != null) {
                                // Newer WebViews route the loadDataWithBaseURL document itself
                                // through here as a data: main-frame request; let it load. Links
                                // are handled by shouldOverrideUrlLoading, and Chromium blocks
                                // page-initiated top-level data: navigations.
                                if (request.isForMainFrame && request.url.scheme.equals("data", ignoreCase = true)) return null
                                // Same scheme + host + port as the registry, GET only; the
                                // restricted client re-checks every redirect hop.
                                val allowed = request.method.equals("GET", ignoreCase = true) &&
                                    RegistryUrls.isAllowedRestrictedResource(request.url.toString(), restrictedBaseUrl)
                                if (!allowed) return blockedResponse()
                            }
                            val scheme = request.url.scheme ?: return if (restricted) blockedResponse() else null
                            if (!scheme.startsWith("http")) return if (restricted) blockedResponse() else null
                            val client: OkHttpClient = restrictedClient ?: ksuApp.okhttpClient
                            val call = client.newCall(
                                Request.Builder()
                                    .url(request.url.toString())
                                    .method(request.method, null)
                                    .headers(request.requestHeaders.toHeaders())
                                    .build()
                            )
                            return try {
                                val reply = call.execute()
                                val header = reply.header("content-type", "text/plain; charset=utf-8")
                                val contentTypes = header?.split(SEMICOLON_SPLIT) ?: emptyList()
                                val mimeType = contentTypes.firstOrNull() ?: "image/*"
                                val charset = contentTypes.getOrNull(1)?.split(EQUALS_SPLIT)?.getOrNull(1) ?: "utf-8"
                                WebResourceResponse(mimeType, charset, reply.body.byteStream())
                            } catch (e: IOException) {
                                Log.e("GithubMarkdown", "Resource load failed", e)
                                if (restricted) return blockedResponse()
                                WebResourceResponse(
                                    "text/html", "utf-8",
                                    ByteArrayInputStream(Log.getStackTraceString(e).toByteArray())
                                )
                            }
                        }
                    }
                    if (restricted) {
                        setOnTouchListener(RestrictedTouchHandler(this))
                    } else setOnTouchListener(object : View.OnTouchListener {
                        private var isHorizontalScrollLocked = false
                        private var initialDownX = 0f
                        private var initialDownY = 0f

                        @SuppressLint("ClickableViewAccessibility")
                        override fun onTouch(v: View, event: MotionEvent): Boolean {
                            when (event.action) {
                                MotionEvent.ACTION_DOWN -> {
                                    initialDownX = event.x
                                    initialDownY = event.y
                                    isHorizontalScrollLocked = false
                                    v.parent.requestDisallowInterceptTouchEvent(true)
                                }

                                MotionEvent.ACTION_MOVE -> {
                                    if (isHorizontalScrollLocked) {
                                        v.parent.requestDisallowInterceptTouchEvent(true)
                                    } else {
                                        val dx = event.x - initialDownX
                                        val dy = event.y - initialDownY
                                        if (abs(dx) > abs(dy)) {
                                            val canScroll = if (dx < 0) scrollInterface.canScrollRight else scrollInterface.canScrollLeft
                                            if (canScroll) {
                                                isHorizontalScrollLocked = true
                                                v.parent.requestDisallowInterceptTouchEvent(true)
                                            } else {
                                                v.parent.requestDisallowInterceptTouchEvent(false)
                                            }
                                        } else {
                                            v.parent.requestDisallowInterceptTouchEvent(false)
                                            return true
                                        }
                                    }
                                }

                                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                                    v.parent.requestDisallowInterceptTouchEvent(false)
                                    isHorizontalScrollLocked = false
                                }
                            }
                            return false
                        }
                    })
                } catch (e: Throwable) {
                    Log.e("GithubMarkdown", "WebView setup failed", e)
                }
            }
            frameLayout.addView(webView)
            frameLayout
        },
        update = { frameLayout ->
            val webView = frameLayout.getChildAt(0) as? WebView ?: return@AndroidView
            webView.settings.textZoom = newTextZoom
            if (webView.tag != html) {
                webView.tag = html
                onLoadingChange(true)
                webView.loadDataWithBaseURL(
                    restrictedBaseUrl ?: APP_ASSETS_ORIGIN, html,
                    "text/html", StandardCharsets.UTF_8.name(), null
                )
            }
        },
        onRelease = { frameLayout ->
            val webView = frameLayout.getChildAt(0) as? WebView
            frameLayout.removeAllViews()
            webView?.apply {
                stopLoading()
                destroy()
            }
        },
        modifier = modifier
            .fillMaxWidth()
            .wrapContentHeight()
            .clipToBounds(),
    )
}

private fun blockedResponse() = WebResourceResponse(
    "text/plain", "utf-8", 403, "Blocked", emptyMap(),
    ByteArrayInputStream(ByteArray(0))
)

private val restrictedClients = java.util.concurrent.ConcurrentHashMap<String, OkHttpClient>()

/**
 * HTTP client for sub-resources of registry content: redirects are only followed while they
 * stay on the registry origin (scheme + host + port); any other hop fails the request.
 */
private fun restrictedResourceClient(origin: String): OkHttpClient = restrictedClients.getOrPut(origin) {
    ksuApp.okhttpClient.newBuilder()
        .followSslRedirects(false)
        .addNetworkInterceptor { chain ->
            val url = chain.request().url.toString()
            if (!RegistryUrls.isAllowedRestrictedResource(url, origin)) {
                throw IOException("Blocked cross-origin resource: ${chain.request().url.host}")
            }
            chain.proceed(chain.request())
        }
        .build()
}

/**
 * WebView that reports when a horizontal drag could not be consumed by the page (the root
 * scroller hit its edge, i.e. no inner `overflow-x: auto` element scrolled). Works without
 * JavaScript, so the restricted renderer can still scroll wide tables and code blocks.
 */
private class EdgeAwareWebView(context: android.content.Context) : WebView(context) {
    var horizontalEdgeReached = false

    override fun onOverScrolled(scrollX: Int, scrollY: Int, clampedX: Boolean, clampedY: Boolean) {
        super.onOverScrolled(scrollX, scrollY, clampedX, clampedY)
        if (clampedX) horizontalEdgeReached = true
    }
}

/**
 * Touch arbitration for the restricted (JavaScript-off) renderer: horizontal drags go to the
 * page first (wide tables / code scroll natively) and are handed to the parent (e.g. a pager)
 * once the page reports it cannot scroll further; vertical drags always go to the parent.
 */
private class RestrictedTouchHandler(
    private val webView: EdgeAwareWebView,
) : View.OnTouchListener {
    private val touchSlop = android.view.ViewConfiguration.get(webView.context).scaledTouchSlop
    private var downX = 0f
    private var downY = 0f
    private var direction = 0 // 0 undecided, 1 horizontal, 2 vertical

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouch(v: View, event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                downX = event.x
                downY = event.y
                direction = 0
                webView.horizontalEdgeReached = false
                v.parent.requestDisallowInterceptTouchEvent(true)
            }

            MotionEvent.ACTION_MOVE -> {
                val dx = event.x - downX
                val dy = event.y - downY
                if (direction == 0 && (abs(dx) > touchSlop || abs(dy) > touchSlop)) {
                    direction = if (abs(dx) > abs(dy)) 1 else 2
                }
                when (direction) {
                    1 -> if (webView.horizontalEdgeReached) v.parent.requestDisallowInterceptTouchEvent(false)
                    2 -> {
                        v.parent.requestDisallowInterceptTouchEvent(false)
                        return true
                    }
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                v.parent.requestDisallowInterceptTouchEvent(false)
                direction = 0
            }
        }
        return false
    }
}

class MarkdownScrollInterface {
    @Volatile
    var canScrollLeft = false

    @Volatile
    var canScrollRight = false

    @JavascriptInterface
    fun updateScrollState(left: Boolean, right: Boolean) {
        canScrollLeft = left
        canScrollRight = right
    }
}

private data class MarkdownColors(
    val bgCode: String,
    val bgRowAlt: String,
    val fgDefault: String,
    val fgLink: String
)

@Composable
private fun getMarkdownColors(containerColor: androidx.compose.ui.graphics.Color?): MarkdownColors {
    val uiMode = LocalUiMode.current

    return when (uiMode) {
        UiMode.Material -> {
            MarkdownColors(
                bgCode = cssColorFromArgb(MaterialTheme.colorScheme.surfaceContainerHigh.toArgb()),
                bgRowAlt = cssColorFromArgb(MaterialTheme.colorScheme.surfaceContainerLow.toArgb()),
                fgDefault = cssColorFromArgb(MaterialTheme.colorScheme.onSurface.toArgb()),
                fgLink = cssColorFromArgb(MaterialTheme.colorScheme.primary.toArgb())
            )
        }

        UiMode.Miuix -> {
            val bgArgb = containerColor?.toArgb() ?: MiuixTheme.colorScheme.surfaceContainer.toArgb()
            val bgLuminance = relativeLuminance(bgArgb)

            fun makeVariant(delta: Float, ratio: Double): Int {
                val candidate = adjustLightnessArgb(bgArgb, delta)
                val madeLighter = delta > 0f
                return ensureVisibleByMix(bgArgb, candidate, ratio, madeLighter)
            }

            val codeDelta = if (bgLuminance > 0.6) -0.05f else 0.05f
            val rowAltDelta = if (bgLuminance > 0.6) -0.02f else 0.02f

            MarkdownColors(
                bgCode = cssColorFromArgb(makeVariant(codeDelta, 1.1)),
                bgRowAlt = cssColorFromArgb(makeVariant(rowAltDelta, 1.05)),
                fgDefault = cssColorFromArgb(MiuixTheme.colorScheme.onSurface.toArgb()),
                fgLink = cssColorFromArgb(MiuixTheme.colorScheme.primary.toArgb())
            )
        }
    }
}
