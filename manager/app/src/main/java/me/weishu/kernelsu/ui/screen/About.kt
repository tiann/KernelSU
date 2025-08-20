package me.weishu.kernelsu.ui.screen

import android.util.Log
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.layout.FixedScale
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.dropUnlessResumed
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.R
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.G2RoundedCornerShape
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical

@Composable
@Destination<RootGraph>
fun AboutScreen(navigator: DestinationsNavigator) {
    val uriHandler = LocalUriHandler.current
    val scrollBehavior = MiuixScrollBehavior()

    val htmlString = stringResource(
        id = R.string.about_source_code,
        "<b><a href=\"https://github.com/tiann/KernelSU\">GitHub</a></b>",
        "<b><a href=\"https://t.me/KernelSU\">Telegram</a></b>"
    )
    val result = extractLinks(htmlString)

    Scaffold(
        topBar = {
            TopAppBar(
                title = stringResource(R.string.about),
                navigationIcon = {
                    IconButton(
                        modifier = Modifier.padding(start = 16.dp),
                        onClick = dropUnlessResumed { navigator.popBackStack() }
                    ) {
                        Icon(
                            imageVector = MiuixIcons.Useful.Back,
                            contentDescription = null,
                            tint = colorScheme.onBackground
                        )
                    }
                },
                scrollBehavior = scrollBehavior
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .height(getWindowSize().height.dp)
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .padding(horizontal = 12.dp),
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 12.dp)
                        .padding(vertical = 48.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier
                            .size(80.dp)
                            .clip(G2RoundedCornerShape(16.dp))
                            .background(Color.White)
                    ) {
                        Image(
                            painter = painterResource(id = R.drawable.ic_launcher_foreground),
                            contentDescription = "icon",
                            contentScale = FixedScale(1f)
                        )
                    }
                    Text(
                        modifier = Modifier.padding(top = 12.dp),
                        text = stringResource(id = R.string.app_name),
                        fontWeight = FontWeight.Medium,
                        fontSize = 26.sp
                    )
                    Text(
                        text = BuildConfig.VERSION_NAME,
                        fontSize = 14.sp
                    )
                }
            }
            item {
                Card(
                    modifier = Modifier.padding(bottom = 12.dp)
                ) {
                    result.forEach {
                        SuperArrow(
                            title = it.fullText,
                            onClick = {
                                uriHandler.openUri(it.url)
                            }
                        )
                    }
                }
                Spacer(
                    Modifier.height(
                        WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                WindowInsets.captionBar.asPaddingValues().calculateBottomPadding()
                    )
                )
            }
        }
    }
}

data class LinkInfo(
    val fullText: String,
    val url: String
)

fun extractLinks(html: String): List<LinkInfo> {
    val regex = Regex(
        """([^<>\n\r]+?)\s*<b>\s*<a\b[^>]*\bhref\s*=\s*(['"]?)([^'"\s>]+)\2[^>]*>([^<]+)</a>\s*</b>\s*(.*?)\s*(?=<br|\n|$)""",
        RegexOption.MULTILINE
    )

    Log.d("ggc", "extractLinks: $html")

    return regex.findAll(html).mapNotNull { match ->
        try {
            val before = match.groupValues[1].trim()
            val url = match.groupValues[3].trim()
            val title = match.groupValues[4].trim()
            val after = match.groupValues[5].trim()

            val fullText = "$before $title $after"
            Log.d("ggc", "extractLinks: $fullText -> $url")
            LinkInfo(fullText, url)
        } catch (e: Exception) {
            Log.e("ggc", "匹配失败: ${e.message}")
            null
        }
    }.toList()
}
