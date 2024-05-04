package me.weishu.kernelsu.ui.screen

import android.app.Activity
import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.StringRes
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.FileUpload
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
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.maxkeppeker.sheets.core.models.base.Header
import com.maxkeppeker.sheets.core.models.base.rememberUseCaseState
import com.maxkeppeler.sheets.list.ListDialog
import com.maxkeppeler.sheets.list.models.ListOption
import com.maxkeppeler.sheets.list.models.ListSelection
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.ramcosta.composedestinations.navigation.EmptyDestinationsNavigator
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.DialogHandle
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rememberCustomDialog
import me.weishu.kernelsu.ui.screen.destinations.FlashScreenDestination
import me.weishu.kernelsu.ui.util.LkmSelection
import me.weishu.kernelsu.ui.util.getCurrentKmi
import me.weishu.kernelsu.ui.util.getSupportedKmis
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
    var installMethod by remember {
        mutableStateOf<InstallMethod?>(null)
    }

    var lkmSelection by remember {
        mutableStateOf<LkmSelection>(LkmSelection.KmiNone)
    }

    val onInstall = {
        installMethod?.let { method ->
            val flashIt = FlashIt.FlashBoot(
                boot = if (method is InstallMethod.SelectFile) method.uri else null,
                lkm = lkmSelection,
                ota = method is InstallMethod.DirectInstallToInactiveSlot
            )
            navigator.navigate(FlashScreenDestination(flashIt))
        }
    }

    val currentKmi by produceState(initialValue = "") { value = getCurrentKmi() }

    val selectKmiDialog = rememberSelectKmiDialog { kmi ->
        kmi?.let {
            lkmSelection = LkmSelection.KmiString(it)
            onInstall()
        }
    }

    val onClickNext = {
        if (lkmSelection == LkmSelection.KmiNone && currentKmi.isBlank()) {
            // no lkm file selected and cannot get current kmi
            selectKmiDialog.show()
        } else {
            onInstall()
        }
    }

    val selectLkmLauncher =
        rememberLauncherForActivityResult(contract = ActivityResultContracts.StartActivityForResult()) {
            if (it.resultCode == Activity.RESULT_OK) {
                it.data?.data?.let { uri ->
                    lkmSelection = LkmSelection.LkmUri(uri)
                }
            }
        }

    val onLkmUpload = {
        selectLkmLauncher.launch(Intent(Intent.ACTION_GET_CONTENT).apply {
            type = "application/octet-stream"
        })
    }

    Scaffold(topBar = {
        TopBar(
            onBack = { navigator.popBackStack() }, onLkmUpload = onLkmUpload
        )
    }) {
        Column(modifier = Modifier.padding(it)) {
            SelectInstallMethod { method ->
                installMethod = method
            }

            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp)
            ) {
                (lkmSelection as? LkmSelection.LkmUri)?.let {
                    Text(
                        stringResource(
                            id = R.string.selected_lkm,
                            it.uri.lastPathSegment ?: "(file)"
                        )
                    )
                }
                Button(modifier = Modifier.fillMaxWidth(),
                    enabled = installMethod != null,
                    onClick = {
                        onClickNext()
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

    data object DirectInstall : InstallMethod() {
        override val label: Int
            get() = R.string.direct_install
    }

    data object DirectInstallToInactiveSlot : InstallMethod() {
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
        id = R.string.select_file_tip, if (isInitBoot()) "init_boot" else "boot"
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
                selectImageLauncher.launch(Intent(Intent.ACTION_GET_CONTENT).apply {
                    type = "application/octet-stream"
                })
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
fun rememberSelectKmiDialog(onSelected: (String?) -> Unit): DialogHandle {
    return rememberCustomDialog { dismiss ->
        val supportedKmi by produceState(initialValue = emptyList<String>()) {
            value = getSupportedKmis()
        }
        val options = supportedKmi.map { value ->
            ListOption(
                titleText = value
            )
        }

        var selection by remember { mutableStateOf<String?>(null) }
        ListDialog(state = rememberUseCaseState(visible = true, onFinishedRequest = {
            onSelected(selection)
        }, onCloseRequest = {
            dismiss()
        }), header = Header.Default(
            title = stringResource(R.string.select_kmi),
        ), selection = ListSelection.Single(
            showRadioButtons = true,
            options = options,
        ) { _, option ->
            selection = option.titleText
        })
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit = {}, onLkmUpload: () -> Unit = {}) {
    TopAppBar(title = { Text(stringResource(R.string.install)) }, navigationIcon = {
        IconButton(
            onClick = onBack
        ) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null) }
    }, actions = {
        IconButton(onClick = onLkmUpload) {
            Icon(Icons.Filled.FileUpload, contentDescription = null)
        }
    })
}

@Composable
@Preview
fun SelectInstall_Preview() {
    InstallScreen(EmptyDestinationsNavigator)
}