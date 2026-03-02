package me.weishu.kernelsu.ui.screen.install

import android.app.Activity
import android.content.Intent
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.DriveFileMove
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.dropUnlessResumed
import me.weishu.kernelsu.R
import me.weishu.kernelsu.getKernelVersion
import me.weishu.kernelsu.ui.component.choosekmidialog.ChooseKmiDialog
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.material.ExpressiveColumn
import me.weishu.kernelsu.ui.component.material.ExpressiveDropdownItem
import me.weishu.kernelsu.ui.component.material.ExpressiveListItem
import me.weishu.kernelsu.ui.component.material.ExpressiveRadioItem
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.util.LkmSelection
import me.weishu.kernelsu.ui.util.getAvailablePartitions
import me.weishu.kernelsu.ui.util.getCurrentKmi
import me.weishu.kernelsu.ui.util.getDefaultPartition
import me.weishu.kernelsu.ui.util.getSlotSuffix
import me.weishu.kernelsu.ui.util.isAbDevice
import me.weishu.kernelsu.ui.util.rootAvailable

/**
 * @author weishu
 * @date 2024/3/12.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InstallScreenMaterial() {
    val navigator = LocalNavigator.current
    val context = LocalContext.current

    var installMethod by remember {
        mutableStateOf<InstallMethod?>(null)
    }

    var lkmSelection by remember {
        mutableStateOf<LkmSelection>(LkmSelection.KmiNone)
    }

    var partitionSelectionIndex by remember { mutableIntStateOf(0) }
    var partitionsState by remember { mutableStateOf<List<String>>(emptyList()) }
    var hasCustomSelected by remember { mutableStateOf(false) }

    val onInstall = {
        installMethod?.let { method ->
            val isOta = method is InstallMethod.DirectInstallToInactiveSlot
            val partitionSelection = partitionsState.getOrNull(partitionSelectionIndex)
            val flashIt = FlashIt.FlashBoot(
                boot = if (method is InstallMethod.SelectFile) method.uri else null,
                lkm = lkmSelection,
                ota = isOta,
                partition = partitionSelection
            )
            navigator.push(Route.Flash(flashIt))
        }
    }

    val currentKmi by produceState(initialValue = "") { value = getCurrentKmi() }

    val showChooseKmiDialog = rememberSaveable { mutableStateOf(false) }
    val chooseKmiDialog = ChooseKmiDialog(showChooseKmiDialog) { kmi ->
        kmi?.let {
            lkmSelection = LkmSelection.KmiString(it)
            onInstall()
        }
    }

    val onClickNext = {
        val isLkmSelected = lkmSelection != LkmSelection.KmiNone
        val isKmiUnknown = currentKmi.isBlank()
        val isSelectFileMode = installMethod is InstallMethod.SelectFile
        if (!isLkmSelected && (isKmiUnknown || isSelectFileMode)) {
            // no lkm file selected and cannot get current kmi or select file mode
            showChooseKmiDialog.value = true
            chooseKmiDialog
        } else {
            onInstall()
        }
    }

    val selectLkmLauncher =
        rememberLauncherForActivityResult(contract = ActivityResultContracts.StartActivityForResult()) {
            if (it.resultCode == Activity.RESULT_OK) {
                it.data?.data?.let { uri ->
                    val isKo = isKoFile(context, uri)
                    if (isKo) {
                        lkmSelection = LkmSelection.LkmUri(uri)
                    } else {
                        lkmSelection = LkmSelection.KmiNone
                        Toast.makeText(
                            context,
                            R.string.install_only_support_ko_file,
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }

    val onLkmUpload = {
        selectLkmLauncher.launch(Intent(Intent.ACTION_GET_CONTENT).apply {
            type = "application/octet-stream"
        })
    }

    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior(rememberTopAppBarState())

    Scaffold(
        topBar = {
            TopBar(
                onBack = dropUnlessResumed { navigator.pop() },
                scrollBehavior = scrollBehavior,
            )
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState())
        ) {
            SelectInstallMethod { method ->
                installMethod = method
            }
            val isOta = installMethod is InstallMethod.DirectInstallToInactiveSlot
            val suffix = produceState(initialValue = "", isOta) {
                value = getSlotSuffix(isOta)
            }.value
            val partitions = produceState(initialValue = emptyList()) {
                value = getAvailablePartitions()
            }.value
            val defaultPartition = produceState(initialValue = "") {
                value = getDefaultPartition()
            }.value
            partitionsState = partitions
            val displayPartitions = partitions.map { name ->
                if (defaultPartition == name) "$name (default)" else name
            }
            val defaultIndex = partitions.indexOf(defaultPartition).takeIf { it >= 0 } ?: 0
            if (!hasCustomSelected) partitionSelectionIndex = defaultIndex
            ExpressiveColumn(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                content = listOf(
                    {
                        if (partitions.isNotEmpty()) {
                            ExpressiveDropdownItem(
                                enabled = installMethod is InstallMethod.DirectInstall || installMethod is InstallMethod.DirectInstallToInactiveSlot,
                                items = displayPartitions,
                                selectedIndex = partitionSelectionIndex,
                                title = "${stringResource(R.string.install_select_partition)} (${suffix})",
                                onItemSelected = { index ->
                                    hasCustomSelected = true
                                    partitionSelectionIndex = index
                                },
                                icon = Icons.Filled.Edit
                            )
                        }
                    },
                    {
                        ExpressiveListItem(
                            leadingContent = { Icon(Icons.AutoMirrored.Filled.DriveFileMove, null) },
                            headlineContent = { Text(stringResource(R.string.install_upload_lkm_file)) },
                            supportingContent = {
                                (lkmSelection as? LkmSelection.LkmUri)?.let {
                                    Text(stringResource(R.string.selected_lkm, it.uri.lastPathSegment ?: "(file)"))
                                }
                            },
                            trailingContent = { Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, null) },
                            onClick = { onLkmUpload() }
                        )
                    }
                )
            )
            Button(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 4.dp),
                enabled = installMethod != null,
                onClick = { onClickNext() }
            ) { Text(stringResource(R.string.install_next)) }
        }
    }
}

@Composable
private fun SelectInstallMethod(onSelected: (InstallMethod) -> Unit = {}) {
    val rootAvailable = rootAvailable()
    val isAbDevice = produceState(initialValue = false) {
        value = isAbDevice()
    }.value
    val defaultPartitionName = produceState(initialValue = "boot") {
        value = getDefaultPartition()
    }.value
    val isGkiDevice = produceState(initialValue = false) {
        value = getKernelVersion().isGKI()
    }.value
    val selectFileTip = stringResource(
        id = R.string.select_file_tip, defaultPartitionName
    )
    val selectFileTipNoGKI = stringResource(id = R.string.select_file_tip_nogki)
    val radioOptions = mutableListOf<InstallMethod>(InstallMethod.SelectFile(summary = if (isGkiDevice) selectFileTip else selectFileTipNoGKI))
    if (rootAvailable && isGkiDevice) {
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

    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            selectedOption = InstallMethod.DirectInstallToInactiveSlot
            onSelected(InstallMethod.DirectInstallToInactiveSlot)
        },
        onDismiss = null
    )
    val dialogTitle = stringResource(android.R.string.dialog_alert_title)
    val dialogContent = stringResource(R.string.install_inactive_slot_warning)

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

    key(isAbDevice) {
        ExpressiveColumn(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
            selectedIndices = radioOptions.indexOfFirst { it.javaClass == selectedOption?.javaClass }.let { if (it >= 0) setOf(it) else emptySet() },
            content = radioOptions.map { option ->
                {
                    ExpressiveRadioItem(
                        title = stringResource(option.label),
                        summary = option.summary,
                        selected = option.javaClass == selectedOption?.javaClass,
                        onClick = { onClick(option) }
                    )
                }
            }
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(
    onBack: () -> Unit = {},
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    TopAppBar(
        title = { Text(stringResource(R.string.install)) },
        navigationIcon = {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null)
            }
        },
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}
