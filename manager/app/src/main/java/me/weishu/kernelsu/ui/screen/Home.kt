import android.os.Build
import android.system.Os
import android.widget.Toast
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Card
import androidx.compose.material3.Snackbar
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.getKernelVersion

@Composable
fun Info(label: String, value: String) {
    Text(
        text = buildAnnotatedString {
            append("$label: ")
            withStyle(
                style = SpanStyle(
                    fontWeight = FontWeight.W500,
                )
            ) {
                append(value)
            }
        },
        softWrap = true,
    )
}

@Composable
fun Home() {

    val statusIcon: Int
    val statusText: String
    val secondaryText: String

    val kernelVersion = getKernelVersion()
    val isManager = Natives.becomeManager(LocalContext.current.packageName)

    if (kernelVersion.isGKI()) {
        // GKI kernel
        if (isManager) {
            statusIcon = R.drawable.ic_status_working
            statusText = "Working"
            secondaryText = "Version: ${Natives.getVersion()}"
        } else {
            statusIcon = R.drawable.ic_status_supported
            statusText = "Not installed"
            secondaryText = "Click to install"
        }
    } else {
        statusIcon = R.drawable.ic_status_unsupported
        statusText = "Unsupported kernel"
        secondaryText = "KernelSU only supports GKI kernels now"
    }

    val context = LocalContext.current

    Column(modifier = Modifier.fillMaxWidth()) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(6.dp)
                .clickable {
                    if (kernelVersion.isGKI() && !isManager) {
                        Toast.makeText(
                            context,
                            "Unimplemented",
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                },
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(10.dp)
            ) {
                Image(
                    painter = painterResource(id = statusIcon),
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
                        text = statusText,
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold
                    )

                    Text(
                        text = secondaryText,
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