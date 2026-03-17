package me.weishu.kernelsu.ui.screen.install

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxHeight
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
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.material.SegmentedCheckboxItem
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedDropdownItem
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.SegmentedRadioItem
import me.weishu.kernelsu.ui.util.LkmSelection

/**
 * @author weishu
 * @date 2024/3/12.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun InstallScreenMaterial(
    uiState: InstallUiState,
    actions: InstallScreenActions,
) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())

    LaunchedEffect(Unit) {
        scrollBehavior.state.heightOffset = scrollBehavior.state.heightOffsetLimit
    }

    Scaffold(
        topBar = {
            TopBar(
                onBack = actions.onBack,
                scrollBehavior = scrollBehavior,
            )
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .fillMaxHeight()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState())
        ) {
            SelectInstallMethod(
                state = uiState,
                onSelected = actions.onSelectMethod,
                onSelectBootImage = actions.onSelectBootImage,
            )

            SegmentedColumn(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                content = buildList {
                    if (uiState.displayPartitions.isNotEmpty()) add {
                        SegmentedDropdownItem(
                            enabled = uiState.canSelectPartition,
                            items = uiState.displayPartitions,
                            selectedIndex = uiState.partitionSelectionIndex,
                            title = "${stringResource(R.string.install_select_partition)} (${uiState.slotSuffix})",
                            onItemSelected = actions.onSelectPartition,
                            icon = Icons.Filled.Edit
                        )
                    }
                    add {
                        SegmentedListItem(
                            leadingContent = {
                                Icon(
                                    Icons.AutoMirrored.Filled.DriveFileMove,
                                    null
                                )
                            },
                            headlineContent = { Text(stringResource(R.string.install_upload_lkm_file)) },
                            supportingContent = {
                                (uiState.lkmSelection as? LkmSelection.LkmUri)?.let {
                                    Text(
                                        stringResource(
                                            R.string.selected_lkm,
                                            it.uri.lastPathSegment ?: "(file)"
                                        )
                                    )
                                }
                            },
                            trailingContent = {
                                if (uiState.lkmSelection is LkmSelection.LkmUri) {
                                    IconButton(onClick = actions.onClearLkm) {
                                        Icon(
                                            Icons.Filled.Close,
                                            contentDescription = stringResource(android.R.string.cancel)
                                        )
                                    }
                                } else {
                                    Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, null)
                                }
                            },
                            onClick = actions.onUploadLkm
                        )
                    }
                }
            )

            SegmentedColumn(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                visibleLen = if (uiState.advancedOptionsShown) 0 else 1,
                content = buildList {
                    val rotationState by animateFloatAsState(
                        targetValue = if (uiState.advancedOptionsShown) 180f else 0f,
                        label = "RotationAnimation"
                    )
                    add {
                        SegmentedListItem(
                            headlineContent = { Text(stringResource(R.string.advanced_options)) },
                            trailingContent = {
                                Icon(
                                    imageVector = Icons.Filled.ExpandMore,
                                    contentDescription = stringResource(R.string.expand),
                                    modifier = Modifier.graphicsLayer { rotationZ = rotationState }
                                )
                            },
                            onClick = actions.onAdvancedOptionsClicked
                        )
                    }
                    add {
                        AnimatedVisibility(
                            uiState.advancedOptionsShown,
                            enter = expandVertically() + fadeIn(),
                            exit = shrinkVertically() + fadeOut()
                        ) {
                            SegmentedCheckboxItem(
                                title = stringResource(id = R.string.allow_shell),
                                summary = stringResource(id = R.string.allow_shell_summary),
                                checked = uiState.allowShell,
                                onCheckedChange = actions.onSelectAllowShell,
                            )
                        }
                    }
                    add {
                        AnimatedVisibility(
                            uiState.advancedOptionsShown,
                            enter = expandVertically() + fadeIn(),
                            exit = shrinkVertically() + fadeOut()
                        ) {
                            SegmentedCheckboxItem(
                                title = stringResource(id = R.string.enable_adb),
                                summary = stringResource(id = R.string.enable_adb_summary),
                                checked = uiState.enableAdb,
                                onCheckedChange = actions.onSelectEnableAdb,
                            )
                        }
                    }
                }
            )
            Button(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 4.dp),
                enabled = uiState.installMethod != null,
                onClick = actions.onNext
            ) { Text(stringResource(R.string.install_next)) }
        }
    }
}

@Composable
private fun SelectInstallMethod(
    state: InstallUiState,
    onSelected: (InstallMethod) -> Unit,
    onSelectBootImage: () -> Unit,
) {
    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            onSelected(InstallMethod.DirectInstallToInactiveSlot)
        },
        onDismiss = null
    )
    val dialogTitle = stringResource(android.R.string.dialog_alert_title)
    val dialogContent = stringResource(R.string.install_inactive_slot_warning)

    val onClick = { option: InstallMethod ->
        when (option) {
            is InstallMethod.SelectFile -> onSelectBootImage()
            is InstallMethod.DirectInstall -> onSelected(option)
            is InstallMethod.DirectInstallToInactiveSlot -> confirmDialog.showConfirm(dialogTitle, dialogContent)
        }
    }

    key(state.installMethodOptions.size) {
        SegmentedColumn(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
            content = state.installMethodOptions.map { option ->
                {
                    SegmentedRadioItem(
                        title = stringResource(option.label),
                        summary = option.summary,
                        selected = option.javaClass == state.installMethod?.javaClass,
                        onClick = { onClick(option) }
                    )
                }
            }
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun TopBar(
    onBack: () -> Unit = {},
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    LargeFlexibleTopAppBar(
        title = { Text(stringResource(R.string.install)) },
        navigationIcon = {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null)
            }
        },
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface,
            scrolledContainerColor = MaterialTheme.colorScheme.surface
        ),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}
