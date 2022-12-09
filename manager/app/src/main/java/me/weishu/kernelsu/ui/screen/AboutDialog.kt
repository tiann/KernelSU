import androidx.compose.foundation.layout.Column
import androidx.compose.material.AlertDialog
import androidx.compose.material.Button
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextAlign
import me.weishu.kernelsu.HyperlinkText

@Composable
fun AboutDialog(openDialog: Boolean, onDismiss: () -> Unit) {

    if (openDialog) {
        AlertDialog(
            onDismissRequest = {
                onDismiss()
            },
            title = {
                Text(text = "About")
            },
            text = {
                Column {

                    HyperlinkText(
                        fullText = "Author: weishu",
                        hyperLinks = mutableMapOf(
                            "weishu" to "https://github.com/tiann",
                        ),
                        textStyle = TextStyle(
                            textAlign = TextAlign.Center,
                            color = Color.Gray
                        ),
                        linkTextColor = MaterialTheme.colors.secondary,
                    )

                    HyperlinkText(
                        fullText = "Github: https://github.com/tiann/KernelSU",
                        hyperLinks = mutableMapOf(
                            "https://github.com/tiann/KernelSU" to "https://github.com/tiann/KernelSU",
                        ),
                        textStyle = TextStyle(
                            textAlign = TextAlign.Center,
                            color = Color.Gray
                        ),
                        linkTextColor = MaterialTheme.colors.secondary,
                    )

                    HyperlinkText(
                        fullText = "Telegram: https://t.me/KernelSU",
                        hyperLinks = mutableMapOf(
                            "https://t.me/KernelSU" to "https://t.me/KernelSU",
                        ),
                        textStyle = TextStyle(
                            textAlign = TextAlign.Center,
                            color = Color.Gray
                        ),
                        linkTextColor = MaterialTheme.colors.secondary,
                    )

                    HyperlinkText(
                        fullText = "QQ: https://pd.qq.com/s/8lipl1brp",
                        hyperLinks = mutableMapOf(
                            "https://pd.qq.com/s/8lipl1brp" to "https://pd.qq.com/s/8lipl1brp",
                        ),
                        textStyle = TextStyle(
                            textAlign = TextAlign.Center,
                            color = Color.Gray
                        ),
                        linkTextColor = MaterialTheme.colors.secondary,
                    )

                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        onDismiss()
                    }) {
                    Text("OK")
                }
            },
        )
    }
}