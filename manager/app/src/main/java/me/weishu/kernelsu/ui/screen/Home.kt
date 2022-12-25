package me.weishu.kernelsu.ui.screen

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
import android.system.Os
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Block
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootNavGraph
import kotlinx.coroutines.launch
import me.weishu.kernelsu.*
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.LinkifyText
import me.weishu.kernelsu.ui.util.LocalSnackbarHost

@OptIn(ExperimentalMaterial3Api::class)
@RootNavGraph(start = true)
@Destination
@Composable
fun HomeScreen() {
    Scaffold(
        topBar = { TopBar() }
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .padding(horizontal = 16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            val kernelVersion = getKernelVersion()
            val isManager = Natives.becomeManager(ksuApp.packageName)
            val ksuVersion = if (isManager) Natives.getVersion() else null

            StatusCard(kernelVersion, ksuVersion)
            InfoCard()
            SupportCard()
            Spacer(Modifier)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar() {
    TopAppBar(
        title = { Text(stringResource(R.string.app_name)) }
    )
}

@Composable
private fun StatusCard(kernelVersion: KernelVersion, ksuVersion: Int?) {
    ElevatedCard(
        colors = CardDefaults.elevatedCardColors(containerColor = run {
            if (ksuVersion != null) MaterialTheme.colorScheme.secondaryContainer
            else MaterialTheme.colorScheme.errorContainer
        })
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable {
                    // TODO: Install kernel
                }
                .padding(24.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            when {
                ksuVersion != null -> {
                    Icon(Icons.Outlined.CheckCircle, stringResource(R.string.home_working))
                    Column(Modifier.padding(start = 20.dp)) {
                        Text(
                            text = stringResource(R.string.home_working),
                            fontFamily = FontFamily.Serif,
                            style = MaterialTheme.typography.titleMedium
                        )
                        Spacer(Modifier.height(4.dp))
                        Text(
                            text = stringResource(R.string.home_working_version, ksuVersion),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
                kernelVersion.isGKI() -> {
                    Icon(Icons.Outlined.Warning, stringResource(R.string.home_not_installed))
                    Column(Modifier.padding(start = 20.dp)) {
                        Text(
                            text = stringResource(R.string.home_not_installed),
                            fontFamily = FontFamily.Serif,
                            style = MaterialTheme.typography.titleMedium
                        )
                        Spacer(Modifier.height(4.dp))
                        Text(
                            text = stringResource(R.string.home_click_to_install),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
                else -> {
                    Icon(Icons.Outlined.Block, stringResource(R.string.home_unsupported))
                    Column(Modifier.padding(start = 20.dp)) {
                        Text(
                            text = stringResource(R.string.home_unsupported),
                            fontFamily = FontFamily.Serif,
                            style = MaterialTheme.typography.titleMedium
                        )
                        Spacer(Modifier.height(4.dp))
                        Text(
                            text = stringResource(R.string.home_unsupported_reason),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun InfoCard() {
    val context = LocalContext.current
    val snackbarHost = LocalSnackbarHost.current
    val scope = rememberCoroutineScope()

    ElevatedCard {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 24.dp, top = 24.dp, end = 24.dp, bottom = 16.dp)
        ) {
            val contents = StringBuilder()
            val uname = Os.uname()

            @Composable
            fun InfoCardItem(label: String, content: String) {
                contents.appendLine(label).appendLine(content).appendLine()
                Text(text = label, style = MaterialTheme.typography.bodyLarge)
                Text(text = content, style = MaterialTheme.typography.bodyMedium)
            }

            InfoCardItem("Kernel", uname.release)

            Spacer(Modifier.height(24.dp))
            InfoCardItem("Arch", uname.machine)

            Spacer(Modifier.height(24.dp))
            InfoCardItem("Version", uname.version)

            Spacer(Modifier.height(24.dp))
            InfoCardItem("API Level", Build.VERSION.SDK_INT.toString())

            Spacer(Modifier.height(24.dp))
            InfoCardItem("ABI", Build.SUPPORTED_ABIS.joinToString(", "))

            Spacer(Modifier.height(24.dp))
            InfoCardItem("Fingerprint", Build.FINGERPRINT)

            Spacer(Modifier.height(24.dp))
            InfoCardItem("Security Patch", Build.VERSION.SECURITY_PATCH)

            val copiedMessage = stringResource(R.string.home_copied_to_clipboard)
            TextButton(
                modifier = Modifier.align(Alignment.End),
                onClick = {
                    val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    cm.setPrimaryClip(ClipData.newPlainText("KernelSU", contents.toString()))
                    scope.launch { snackbarHost.showSnackbar(copiedMessage) }
                },
                content = { Text(stringResource(android.R.string.copy)) }
            )
        }
    }
}

@Preview
@Composable
private fun StatusCardPreview() {
    Column {
        StatusCard(KernelVersion(5, 10, 101), 1)
        StatusCard(KernelVersion(5, 10, 101), null)
        StatusCard(KernelVersion(4, 10, 101), null)
    }
}

@Preview
@Composable
private fun SupportCard() {
    ElevatedCard {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp)
        ) {
            Text(
                text = stringResource(R.string.home_support),
                fontWeight = FontWeight.SemiBold,
                style = MaterialTheme.typography.titleMedium
            )
            Spacer(Modifier.height(8.dp))
            CompositionLocalProvider(LocalTextStyle provides MaterialTheme.typography.bodyMedium) {
                LinkifyText("Author: weishu")
                LinkifyText("Github: https://github.com/tiann/KernelSU")
                LinkifyText("Telegram: https://t.me/KernelSU")
                LinkifyText("QQ: https://pd.qq.com/s/8lipl1brp")
            }
        }
    }
}
