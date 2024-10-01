package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.scale
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextLinkStyles
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.fromHtml
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.R

@Preview
@Composable
fun AboutCard() {
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp)
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
fun AboutDialog(dismiss: () -> Unit) {
    Dialog(
        onDismissRequest = { dismiss() }
    ) {
        AboutCard()
    }
}

@Composable
private fun AboutCardContent() {
    Column(
        modifier = Modifier.fillMaxWidth()
    ) {
        Row {
            Surface(
                modifier = Modifier.size(40.dp),
                color = colorResource(id = R.color.ic_launcher_background),
                shape = CircleShape
            ) {
                Image(
                    painter = painterResource(id = R.drawable.ic_launcher_foreground),
                    contentDescription = "icon",
                    modifier = Modifier.scale(1.4f)
                )
            }

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

                val annotatedString = AnnotatedString.Companion.fromHtml(
                    htmlString = stringResource(
                        id = R.string.about_source_code,
                        "<b><a href=\"https://github.com/tiann/KernelSU\">GitHub</a></b>",
                        "<b><a href=\"https://t.me/KernelSU\">Telegram</a></b>"
                    ),
                    linkStyles = TextLinkStyles(
                        style = SpanStyle(
                            color = MaterialTheme.colorScheme.primary,
                            textDecoration = TextDecoration.Underline
                        ),
                        pressedStyle = SpanStyle(
                            color = MaterialTheme.colorScheme.primary,
                            background = MaterialTheme.colorScheme.secondaryContainer,
                            textDecoration = TextDecoration.Underline
                        )
                    )
                )
                Text(
                    text = annotatedString,
                    style = TextStyle(
                        fontSize = 14.sp
                    )
                )
            }
        }
    }
}