import android.text.method.LinkMovementMethod
import android.text.util.Linkify
import android.util.Patterns
import android.widget.TextView
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.text.util.LinkifyCompat
import me.weishu.kernelsu.LinkifyText

@Composable
fun DefaultLinkifyText(modifier: Modifier = Modifier, text: String?) {
    val context = LocalContext.current
    val customLinkifyTextView = remember {
        TextView(context)
    }
    AndroidView(modifier = modifier, factory = { customLinkifyTextView }) { textView ->
        textView.text = text ?: ""
        LinkifyCompat.addLinks(textView, Linkify.ALL)
        Linkify.addLinks(textView, Patterns.PHONE,"tel:",
            Linkify.sPhoneNumberMatchFilter, Linkify.sPhoneNumberTransformFilter)
        textView.movementMethod = LinkMovementMethod.getInstance()
    }
}
@Composable
fun AboutDialog(openDialog: Boolean, onDismiss: () -> Unit) {

    if (!openDialog) {
        return
    }

    AlertDialog(
        onDismissRequest = {
            onDismiss()
        },
        title = {
            Text(text = "About")
        },
        text = {
            Column {
                LinkifyText(text = "Author: weishu")
                LinkifyText(text = "Github: https://github.com/tiann/KernelSU")
                LinkifyText(text = "Telegram: https://t.me/KernelSU")
                LinkifyText(text = "QQ: https://pd.qq.com/s/8lipl1brp")
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    onDismiss()
                }) {
                Text("OK")
            }
        },
    )

}

@Preview
@Composable
fun Preview_AboutDialog() {
    AboutDialog(true, {})
}