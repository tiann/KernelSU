package me.weishu.kernelsu.ui.screen

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.StringRes
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rememberLoadingDialog
import me.weishu.kernelsu.ui.screen.destinations.FlashScreenDestination
import me.weishu.kernelsu.ui.util.DownloadListener
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.getLKMUrl
import me.weishu.kernelsu.ui.util.isAbDevice
import me.weishu.kernelsu.ui.util.isInitBoot
import me.weishu.kernelsu.ui.util.rootAvailable

/**
 * @author weishu
 * @date 2024/3/12.
 */
@Destination
@Composable
fun InstallScreen(navigator: DestinationsNavigator) {
    val scope = rememberCoroutineScope()
    val loadingDialog = rememberLoadingDialog()
    val context = LocalContext.current
    var installMethod by remember {
        mutableStateOf<InstallMethod?>(null)
    }

    val onFileDownloaded = { uri: Uri ->

        installMethod?.let {
            scope.launch(Dispatchers.Main) {
                when (it) {
                    InstallMethod.DirectInstall -> {
                        navigator.navigate(
                            FlashScreenDestination(
                                FlashIt.FlashBoot(
                                    null,
                                    uri,
                                    false
                                )
                            )
                        )
                    }

                    InstallMethod.DirectInstallToInactiveSlot -> {
                        navigator.navigate(
                            FlashScreenDestination(
                                FlashIt.FlashBoot(
                                    null,
                                    uri,
                                    true
                                )
                            )
                        )
                    }

                    is InstallMethod.SelectFile -> {
                        navigator.navigate(
                            FlashScreenDestination(
                                FlashIt.FlashBoot(
                                    it.uri,
                                    uri,
                                    false
                                )
                            )
                        )
                    }
                }
            }
        }
    }

    Scaffold(topBar = {
        TopBar {
            navigator.popBackStack()
        }
    }) {
        Column(modifier = Modifier.padding(it)) {
            SelectInstallMethod { method ->
                installMethod = method
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp)
            ) {

                DownloadListener(context = context) { uri ->
                    onFileDownloaded(uri)
                    loadingDialog.hide()
                }

                val failedMessage = stringResource(id = R.string.failed_to_fetch_lkm_url)
                val downloadingMessage = stringResource(id = R.string.downloading)
                Button(
                    modifier = Modifier.fillMaxWidth(),
                    enabled = installMethod != null,
                    onClick = {
                        loadingDialog.showLoading()
                        scope.launch(Dispatchers.IO) {
                            getLKMUrl().onFailure { throwable ->
                                loadingDialog.hide()
                                scope.launch(Dispatchers.Main) {
                                    Toast.makeText(
                                        context,
                                        failedMessage.format(throwable.message),
                                        Toast.LENGTH_SHORT
                                    ).show()
                                }
                            }.onSuccess { result ->
                                download(
                                    context = context,
                                    url = result.second,
                                    fileName = result.first,
                                    description = downloadingMessage.format(
                                        result.first
                                    ),
                                    onDownloaded = { uri ->
                                        onFileDownloaded(uri)
                                        loadingDialog.hide()
                                    },
                                    onDownloading = {}
                                )
                            }
                        }
                    }) {
                    Text(
                        stringResource(id = R.string.install_next),
                        fontSize = MaterialTheme.typography.bodyMedium.fontSize
                    )
                }
            }
        }
    }
}

sealed class InstallMethod {
    data class SelectFile(
        val uri: Uri? = null,
        @StringRes override val label: Int = R.string.select_file,
        override val summary: String?
    ) : InstallMethod()

    object DirectInstall : InstallMethod() {
        override val label: Int
            get() = R.string.direct_install
    }

    object DirectInstallToInactiveSlot : InstallMethod() {
        override val label: Int
            get() = R.string.install_inactive_slot
    }

    abstract val label: Int
    open val summary: String? = null
}

@Composable
private fun SelectInstallMethod(onSelected: (InstallMethod) -> Unit = {}) {
    val rootAvailable = rootAvailable()
    val isAbDevice = isAbDevice()
    val selectFileTip = stringResource(
        id = R.string.select_file_tip,
        if (isInitBoot()) "init_boot" else "boot"
    )
    val radioOptions =
        mutableListOf<InstallMethod>(InstallMethod.SelectFile(summary = selectFileTip))
    if (rootAvailable) {
        radioOptions.add(InstallMethod.DirectInstall)

        if (isAbDevice) {
            radioOptions.add(InstallMethod.DirectInstallToInactiveSlot)
        }
    }

    var selectedOption by remember { mutableStateOf<InstallMethod?>(null) }
    val selectImageLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) {
        if (it.resultCode == Activity.RESULT_OK) {
            it.data?.data?.let { uri ->
                val option = InstallMethod.SelectFile(uri, summary = selectFileTip)
                selectedOption = option
                onSelected(option)
            }
        }
    }

    val confirmDialog = rememberConfirmDialog(onConfirm = {
        selectedOption = InstallMethod.DirectInstallToInactiveSlot
        onSelected(InstallMethod.DirectInstallToInactiveSlot)
    }, onDismiss = null)
    val dialogTitle = stringResource(id = android.R.string.dialog_alert_title)
    val dialogContent = stringResource(id = R.string.install_inactive_slot_warning)

    val onClick = { option: InstallMethod ->

        when (option) {
            is InstallMethod.SelectFile -> {
                selectImageLauncher.launch(
                    Intent(Intent.ACTION_GET_CONTENT).apply {
                        type = "application/octet-stream"
                    }
                )
            }

            is InstallMethod.DirectInstall -> {
                selectedOption = option
                onSelected(option)
            }

            is InstallMethod.DirectInstallToInactiveSlot -> {
                confirmDialog.showConfirm(dialogTitle, dialogContent)
            }
        }
    }

    Column {
        radioOptions.forEach { option ->
            Row(verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable {
                        onClick(option)
                    }) {
                RadioButton(selected = option.javaClass == selectedOption?.javaClass, onClick = {
                    onClick(option)
                })
                Column {
                    Text(
                        text = stringResource(id = option.label),
                        fontSize = MaterialTheme.typography.titleMedium.fontSize,
                        fontFamily = MaterialTheme.typography.titleMedium.fontFamily,
                        fontStyle = MaterialTheme.typography.titleMedium.fontStyle
                    )
                    option.summary?.let {
                        Text(
                            text = it,
                            fontSize = MaterialTheme.typography.bodySmall.fontSize,
                            fontFamily = MaterialTheme.typography.bodySmall.fontFamily,
                            fontStyle = MaterialTheme.typography.bodySmall.fontStyle
                        )
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit = {}) {
    TopAppBar(
        title = { Text(stringResource(R.string.install)) },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
        },
    )
}

@Composable
@Preview
fun SelectInstall_Preview() {
//    InstallScreen(DestinationsNavigator())
}