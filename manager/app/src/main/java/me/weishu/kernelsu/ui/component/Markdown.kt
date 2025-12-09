package me.weishu.kernelsu.ui.component

import android.graphics.text.LineBreaker
import android.os.Build
import android.text.Layout
import android.text.method.LinkMovementMethod
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.viewinterop.AndroidView
import io.noties.markwon.Markwon
import io.noties.markwon.utils.NoCopySpannableFactory
import top.yukonga.miuix.kmp.theme.MiuixTheme

private const val TEXTVIEW_TAG = "markdownTextView"

@Composable
fun Markdown(content: String) {
    val contentColor = MiuixTheme.colorScheme.onBackground.toArgb()

    AndroidView(
        factory = { context ->
            val frameLayout = FrameLayout(context)
            val scrollView = ScrollView(context)
            val textView = TextView(context).apply {
                tag = TEXTVIEW_TAG
                movementMethod = LinkMovementMethod.getInstance()
                setSpannableFactory(NoCopySpannableFactory.getInstance())
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    breakStrategy = LineBreaker.BREAK_STRATEGY_SIMPLE
                }
                hyphenationFrequency = Layout.HYPHENATION_FREQUENCY_NONE
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                )
            }
            scrollView.addView(textView)
            frameLayout.addView(scrollView)
            frameLayout
        },
        modifier = Modifier
            .fillMaxWidth()
            .wrapContentHeight()
            .clipToBounds(),
        update = { frameLayout ->
            frameLayout.findViewWithTag<TextView>(TEXTVIEW_TAG)?.let { textView ->
                Markwon.create(textView.context).setMarkdown(textView, content)
                textView.setTextColor(contentColor)
            }
        }
    )
}
