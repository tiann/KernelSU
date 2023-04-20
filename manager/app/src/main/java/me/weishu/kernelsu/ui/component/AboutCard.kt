package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.core.content.res.ResourcesCompat
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import com.ireward.htmlcompose.HtmlText
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.R

@Preview
@Composable
fun AboutCard() {
    ElevatedCard(
        modifier = Modifier
            .fillMaxWidth(),
        shape = RoundedCornerShape(8.dp),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp)
        ) {
            AboutCardContent()
        }
    }
}

@Composable
fun AboutDialog(showAboutDialog: MutableState<Boolean>) {
    if (showAboutDialog.value) {
        Dialog(onDismissRequest = { showAboutDialog.value = false }) {
            AboutCard()
        }
    }
}

@Composable
private fun AboutCardContent() {
    Column(
        modifier = Modifier
            .fillMaxWidth()
    ) {
        CompositionLocalProvider(LocalTextStyle provides MaterialTheme.typography.bodyMedium) {

            val drawable = ResourcesCompat.getDrawable(
                LocalContext.current.resources,
                R.mipmap.ic_launcher,
                LocalContext.current.theme
            )

            Row {
                Image(
                    painter = rememberDrawablePainter(drawable),
                    contentDescription = "icon",
                    modifier = Modifier.size(40.dp)
                )

                Spacer(modifier = Modifier.width(12.dp))

                Column {

                    Text(
                        stringResource(id = R.string.app_name),
                        style = MaterialTheme.typography.titleSmall,
                        fontSize = 18.sp
                    )
                    Text(
                        BuildConfig.VERSION_NAME,
                        style = MaterialTheme.typography.bodySmall,
                        fontSize = 14.sp
                    )

                    Spacer(modifier = Modifier.height(8.dp))

                    val uriHandler = LocalUriHandler.current
                    HtmlText(
                        text = stringResource(
                            id = R.string.about_source_code,
                            "<b><a href=\"https://github.com/tiann/KernelSU\">GitHub</a></b>",
                            "<b><a href=\"https://t.me/KernelSU\">Telegram</a></b>"
                        ),
                        linkClicked = {
                            uriHandler.openUri(it)
                        },
                        style = MaterialTheme.typography.bodyMedium,
                        fontSize = 16.sp,
                    )
                }
            }
        }
    }
}