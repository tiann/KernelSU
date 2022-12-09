import android.os.Build
import android.system.Os
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material.Card
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Done
import androidx.compose.material.icons.filled.Warning
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ClipboardManager
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.weishu.kernelsu.Natives
import java.util.*

@Composable
fun Info(label: String, value: String) {
    Text(
        buildAnnotatedString {
            append("$label: ")
            withStyle(
                style = SpanStyle(
                    fontWeight = FontWeight.W900,
                    color = MaterialTheme.colors.secondary
                )
            ) {
                append(value)
            }
        }
    )
}

@Composable
fun Home() {

    val isManager = Natives.becomeManager()

    Column(modifier = Modifier.fillMaxWidth()) {

        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(6.dp)
                .clickable { },
            elevation = 10.dp,
            backgroundColor = MaterialTheme.colors.secondary
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(10.dp)
            ) {
                Image(
                    if (isManager) Icons.Filled.Done else Icons.Filled.Warning,
                    null,
                    modifier = Modifier
                        .size(64.dp)
                )
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 10.dp),
                ) {
                    Text(
                        text = if (isManager) "Installed" else "Not Installed",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold
                    )

                    Text(
                        text = if (isManager) "Version: ${Natives.getVersion()}" else "Click to Install",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Normal
                    )
                }
            }

        }

        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(6.dp)
                .clickable { },
            elevation = 10.dp
        ) {
            Column(
                modifier = Modifier.padding(10.dp)
            ) {

                Os.uname().let { uname ->
                    Info("Kernel", uname.release)
                    Info("Arch", uname.machine)
                    Info("Version", uname.version)
                }

                Info("API Level", Build.VERSION.SDK_INT.toString())

                Info("ABI", Build.SUPPORTED_ABIS.joinToString(", "))

                Info("Fingerprint", Build.FINGERPRINT)

                Info("Security Patch", Build.VERSION.SECURITY_PATCH)

            }
        }

    }

}