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
import androidx.compose.material3.surfaceColorAtElevation
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.webkit.WebViewAssetLoader
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

private val SEMICOLON_SPLIT = ";\\s*".toRegex()
private val EQUALS_SPLIT = "=\\s*".toRegex()

@SuppressLint("JavascriptInterface", "SetJavaScriptEnabled", "WrongConstant")
@Composable
fun GithubMarkdown(
    content: String,
    isMarkdown: Boolean = false,
    onLoadingChange: (Boolean) -> Unit = {},
    containerColor: androidx.compose.ui.graphics.Color? = null,
) {
    val density = LocalDensity.current
    val systemDensity = LocalResources.current.displayMetrics.density
    val fontScale = density.fontScale
    val pageScale = density.density / systemDensity
    val newTextZoom = (90 * pageScale * fontScale).toInt()
    val scrollInterface = remember { MarkdownScrollInterface() }

    val isDark = isInDarkTheme()
    val dir = if (LocalLayoutDirection.current == LayoutDirection.Rtl) "rtl" else "ltr"

    val colors = getMarkdownColors(containerColor)
    val bgDefault = colors.bgDefault
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
    val renderer = remember(extensions) { HtmlRenderer.builder().extensions(extensions).build() }
    val rendered = remember(content, isMarkdown) {
        if (isMarkdown) renderer.render(parser.parse(content)) else content
    }
    val body = """
        <style>
         :root {
             --background: $bgDefault;
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
        </style>
        $rendered
    """.trimIndent()
    val html = template
        .replace("@dir@", dir)
        .replace("@body@", body)

    AndroidView(
        factory = { context ->
            val frameLayout = FrameLayout(context)
            val webView = WebView(context).apply {
                try {
                    setBackgroundColor(Color.TRANSPARENT)
                    isVerticalScrollBarEnabled = false
                    isHorizontalScrollBarEnabled = false

                    settings.apply {
                        offscreenPreRaster = true
                        javaScriptEnabled = true
                        domStorageEnabled = true
                        mixedContentMode = WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
                        allowContentAccess = false
                        allowFileAccess = false
                        cacheMode = WebSettings.LOAD_CACHE_ELSE_NETWORK
                        textZoom = newTextZoom
                        setSupportZoom(false)
                        setGeolocationEnabled(false)
                    }
                    layoutParams = FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT
                    )
                    addJavascriptInterface(scrollInterface, "AndroidScroll")
                    webViewClient = object : WebViewClient() {
                        private val assetLoader = WebViewAssetLoader.Builder()
                            .addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(context))
                            .build()

                        override fun onPageFinished(view: WebView, url: String) {
                            super.onPageFinished(view, url)

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
                            val scheme = request.url.scheme ?: return null
                            if (!scheme.startsWith("http")) return null
                            val client: OkHttpClient = ksuApp.okhttpClient
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
                                WebResourceResponse(
                                    "text/html", "utf-8",
                                    ByteArrayInputStream(Log.getStackTraceString(e).toByteArray())
                                )
                            }
                        }
                    }
                    setOnTouchListener(object : View.OnTouchListener {
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
                    loadDataWithBaseURL(
                        "https://appassets.androidplatform.net", html,
                        "text/html", StandardCharsets.UTF_8.name(), null
                    )
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
            onLoadingChange(true)
            webView.loadDataWithBaseURL(
                "https://appassets.androidplatform.net", html,
                "text/html", StandardCharsets.UTF_8.name(), null
            )
        },
        onRelease = { frameLayout ->
            val webView = frameLayout.getChildAt(0) as? WebView
            frameLayout.removeAllViews()
            webView?.apply {
                stopLoading()
                destroy()
            }
        },
        modifier = Modifier
            .fillMaxWidth()
            .wrapContentHeight()
            .clipToBounds(),
    )
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
    val bgDefault: String,
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
            val bgArgb = containerColor?.toArgb() ?: MaterialTheme.colorScheme.surfaceColorAtElevation(1.dp).toArgb()

            MarkdownColors(
                bgDefault = cssColorFromArgb(bgArgb),
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
                bgDefault = cssColorFromArgb(bgArgb),
                bgCode = cssColorFromArgb(makeVariant(codeDelta, 1.1)),
                bgRowAlt = cssColorFromArgb(makeVariant(rowAltDelta, 1.05)),
                fgDefault = cssColorFromArgb(MiuixTheme.colorScheme.onSurface.toArgb()),
                fgLink = cssColorFromArgb(MiuixTheme.colorScheme.primary.toArgb())
            )
        }
    }
}
