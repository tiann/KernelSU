package me.weishu.kernelsu.ui.component

import android.content.Context
import android.graphics.Color
import android.os.Build
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.net.toUri
import androidx.webkit.WebViewAssetLoader
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.adjustLightnessArgb
import me.weishu.kernelsu.ui.util.cssColorFromArgb
import me.weishu.kernelsu.ui.util.ensureVisibleByMix
import me.weishu.kernelsu.ui.util.relativeLuminance
import okhttp3.Headers.Companion.toHeaders
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okio.IOException
import top.yukonga.miuix.kmp.theme.MiuixTheme
import java.io.ByteArrayInputStream
import java.nio.charset.StandardCharsets

@Composable
fun GithubMarkdown(content: String) {
    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val themeMode = prefs.getInt("color_mode", 0)
    val isDark = isInDarkTheme(themeMode)
    val dir = if (LocalLayoutDirection.current == LayoutDirection.Rtl) "rtl" else "ltr"

    val bgArgb = MiuixTheme.colorScheme.surfaceContainer.toArgb()
    val bgLuminance = relativeLuminance(bgArgb)
    val minVisibleRatio = 1.15

    fun makeVariant(delta: Float): Int {
        val candidate = adjustLightnessArgb(bgArgb, delta)
        val madeLighter = delta > 0f
        return ensureVisibleByMix(bgArgb, candidate, minVisibleRatio, madeLighter)
    }

    val bgDefault = cssColorFromArgb(bgArgb)
    val bgMuted = cssColorFromArgb(makeVariant(if (bgLuminance > 0.6) -0.06f else 0.06f))
    val bgNeutralMuted = cssColorFromArgb(makeVariant(if (bgLuminance > 0.6) -0.12f else 0.12f))
    val bgAttentionMuted = cssColorFromArgb(makeVariant(-0.12f))
    val fgLink = cssColorFromArgb(MiuixTheme.colorScheme.primary.toArgb())

    val cssHref = "https://appassets.androidplatform.net/assets/github-markdown.css"
    val html = """
        <!DOCTYPE html>
        <html>
        <head>
          <meta charset='utf-8'/>
          <meta name='viewport' content='width=device-width, initial-scale=1'/>
          <link rel="stylesheet" href="$cssHref" />
          <style>
            html, body { margin:0; padding:0; background:transparent; }
            img, video { max-width:100%; height:auto; }
            pre { white-space: pre-wrap; }
            * { scrollbar-width: none; }
            *::-webkit-scrollbar { width: 0 !important; height: 0 !important; display: none; }
            .markdown-body {
              box-sizing: border-box;
              min-width: 200px;
              max-width: 980px;
              margin: 0 auto;
              padding: 45px;
              --bgColor-default: $bgDefault;
              --bgColor-muted: $bgMuted;
              --bgColor-neutral-muted: $bgNeutralMuted;
              --bgColor-attention-muted: $bgAttentionMuted;
              --fgColor-accent: $fgLink;
            }
            @media (max-width: 767px) {
              .markdown-body { padding: 15px; }
            }
          </style>
        </head>
        <body dir='${dir}'>
          <article class='markdown-body'>${content}</article>
        </body>
        </html>
    """.trimIndent()

    AndroidView(
        factory = { context ->
            WebView(context).apply {
                setBackgroundColor(Color.TRANSPARENT)
                isVerticalScrollBarEnabled = false
                isHorizontalScrollBarEnabled = false
                overScrollMode = android.view.View.OVER_SCROLL_NEVER
                settings.apply {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        setForceDark(if (isDark) WebSettings.FORCE_DARK_ON else WebSettings.FORCE_DARK_OFF)
                    }
                    offscreenPreRaster = true
                    domStorageEnabled = true
                    mixedContentMode = WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
                    allowContentAccess = false
                    allowFileAccessFromFileURLs = true
                    allowFileAccess = false
                    setSupportZoom(false)
                    cacheMode = WebSettings.LOAD_CACHE_ELSE_NETWORK
                    textZoom = 80
                }
                webViewClient = object : WebViewClient() {
                    private val assetLoader = WebViewAssetLoader.Builder().addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(context)).build()

                    override fun shouldOverrideUrlLoading(
                        view: WebView, request: WebResourceRequest
                    ): Boolean {
                        // Open external URLs via system
                        val url = request.url.toString()
                        try {
                            val intent = android.content.Intent(
                                android.content.Intent.ACTION_VIEW, url.toUri()
                            )
                            intent.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                            context.startActivity(intent)
                        } catch (_: Throwable) {
                        }
                        return true
                    }

                    override fun shouldInterceptRequest(
                        view: WebView, request: WebResourceRequest
                    ): WebResourceResponse? {
                        assetLoader.shouldInterceptRequest(request.url)?.let { return it }
                        val scheme = request.url.scheme ?: return null
                        if (!scheme.startsWith("http")) return null
                        val client: OkHttpClient = ksuApp.okhttpClient
                        val call = client.newCall(
                            Request.Builder().url(request.url.toString()).method(request.method, null).headers(request.requestHeaders.toHeaders()).build()
                        )
                        return try {
                            val reply: Response = call.execute()
                            val header = reply.header("content-type", "text/plain; charset=utf-8")
                            val contentTypes = header?.split(";\\s*".toRegex()) ?: emptyList()
                            val mimeType = contentTypes.firstOrNull() ?: "image/*"
                            val charset = contentTypes.getOrNull(1)?.split("=\\s*".toRegex())?.getOrNull(1) ?: "utf-8"
                            val body = reply.body ?: return null
                            WebResourceResponse(
                                mimeType, charset, body.byteStream()
                            )
                        } catch (e: IOException) {
                            WebResourceResponse(
                                "text/html", "utf-8", ByteArrayInputStream(
                                    android.util.Log.getStackTraceString(e).toByteArray(StandardCharsets.UTF_8)
                                )
                            )
                        }
                    }
                }
                loadDataWithBaseURL(
                    "https://appassets.androidplatform.net", html, "text/html", StandardCharsets.UTF_8.name(), null
                )
            }
        },
        modifier = Modifier
            .fillMaxWidth()
            .clipToBounds(),
    )
}
