package me.weishu.kernelsu.ui.screen.module

import android.annotation.SuppressLint
import android.app.Activity.RESULT_OK
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandHorizontally
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkHorizontally
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.outlined.Cloud
import androidx.compose.material.icons.outlined.Code
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Download
import androidx.compose.material.icons.outlined.PlayArrow
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonColors
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.ProvideTextStyle
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarResult
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.layout.FixedScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.data.model.ModuleUpdateInfo
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.component.material.ExpressiveSwitch
import me.weishu.kernelsu.ui.component.material.SearchAppBar
import me.weishu.kernelsu.ui.component.material.TonalCard
import me.weishu.kernelsu.ui.component.rebootlistpopup.RebootListPopup
import me.weishu.kernelsu.ui.component.statustag.StatusTag
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.util.reboot

@SuppressLint("StringFormatInvalid")
@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun ModulePagerMaterial(
    uiState: ModuleUiState,
    confirmDialogState: ModuleConfirmDialogState?,
    effect: ModuleEffect?,
    actions: ModuleActions,
    bottomInnerPadding: Dp,
) {
    val snackBarHost = LocalSnackbarHost.current
    val haptic = LocalHapticFeedback.current

    val context = LocalContext.current
    val resource = LocalResources.current

    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())

    val pullToRefreshState = rememberPullToRefreshState()

    val listState = rememberLazyListState()
    val searchListState = rememberLazyListState()
    val threshold = with(LocalDensity.current) { 100.dp.toPx() }
    val fabExpanded by remember {
        var lastIndex = 0
        var lastOffset = 0
        var scrollDelta = 0f
        var expanded = true
        derivedStateOf {
            val currentIndex = listState.firstVisibleItemIndex
            val currentOffset = listState.firstVisibleItemScrollOffset
            val delta = if (currentIndex == lastIndex) {
                (currentOffset - lastOffset).toFloat()
            } else if (currentIndex > lastIndex) {
                100f
            } else {
                -100f
            }
            scrollDelta = (scrollDelta + delta).coerceIn(-threshold, threshold)
            lastIndex = currentIndex
            lastOffset = currentOffset
            if (currentIndex == 0) {
                expanded = true
                scrollDelta = 0f
            } else if (expanded && scrollDelta >= threshold) {
                expanded = false
                scrollDelta = 0f
            } else if (!expanded && scrollDelta <= -threshold) {
                expanded = true
                scrollDelta = 0f
            }
            expanded
        }
    }

    val shortcutState = rememberModuleShortcutState(context)
    val showShortcutDialog = remember { mutableStateOf(false) }
    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            when (val request = confirmDialogState?.request) {
                is ModuleConfirmRequest.Uninstall -> actions.onUninstallModule(request.module)
                is ModuleConfirmRequest.Update -> actions.onConfirmUpdate(request)
                null -> Unit
            }
        },
        onDismiss = actions.onDismissConfirmRequest,
    )

    fun openShortcutDialogForType(type: ShortcutType) {
        shortcutState.selectType(type)
        showShortcutDialog.value = true
    }

    val pickShortcutIconLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri ->
        shortcutState.updateIconUri(uri?.toString())
    }

    fun onModuleAddShortcut(module: Module, type: ShortcutType) {
        shortcutState.bindModule(module)
        openShortcutDialogForType(type)
    }

    LaunchedEffect(confirmDialogState) {
        confirmDialogState?.let {
            confirmDialog.showConfirm(
                title = it.title,
                content = it.content,
                markdown = it.markdown,
                html = it.html,
                confirm = it.confirm,
                dismiss = it.dismiss,
            )
        }
    }

    LaunchedEffect(effect) {
        when (effect) {
            is ModuleEffect.Toast -> {
                Toast.makeText(context, effect.message, Toast.LENGTH_SHORT).show()
                actions.onConsumeEffect()
            }

            is ModuleEffect.SnackBar -> {
                snackBarHost.currentSnackbarData?.dismiss()
                val result = snackBarHost.showSnackbar(
                    message = effect.message,
                    actionLabel = resource.getString(R.string.reboot),
                    duration = SnackbarDuration.Long
                )
                if (result == SnackbarResult.ActionPerformed) {
                    reboot()
                }
                actions.onConsumeEffect()
            }

            null -> Unit
        }
    }

    Scaffold(
        topBar = {
            SearchAppBar(
                title = { Text(stringResource(R.string.module)) },
                searchText = uiState.searchStatus.searchText,
                onSearchTextChange = actions.onSearchTextChange,
                onClearClick = actions.onClearSearch,
                navigationIcon = {
                    IconButton(
                        onClick = { actions.onOpenRepo() }
                    ) {
                        Icon(
                            imageVector = Icons.Outlined.Cloud,
                            contentDescription = stringResource(id = R.string.module_repos)
                        )
                    }
                },
                actions = {
                    RebootListPopup()

                    var showDropdown by remember { mutableStateOf(false) }
                    IconButton(
                        onClick = { showDropdown = true }
                    ) {
                        Icon(
                            imageVector = Icons.Filled.MoreVert,
                            contentDescription = stringResource(id = R.string.settings)
                        )
                        DropdownMenu(
                            expanded = showDropdown,
                            onDismissRequest = { showDropdown = false }
                        ) {
                            DropdownMenuItem(
                                text = { Text(stringResource(R.string.module_sort_action_first)) },
                                trailingIcon = { Checkbox(uiState.sortActionFirst, null) },
                                onClick = {
                                    haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                    actions.onToggleSortActionFirst()
                                }
                            )
                            DropdownMenuItem(
                                text = { Text(stringResource(R.string.module_sort_enabled_first)) },
                                trailingIcon = { Checkbox(uiState.sortEnabledFirst, null) },
                                onClick = {
                                    haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                                    actions.onToggleSortEnabledFirst()
                                }
                            )
                        }
                    }
                },
                scrollBehavior = scrollBehavior,
                searchContent = { bottomPadding, closeSearch ->
                    LaunchedEffect(uiState.searchStatus.searchText) {
                        searchListState.scrollToItem(0)
                    }
                    ModuleList(
                        bottomInnerPadding = bottomPadding,
                        modifier = Modifier.fillMaxSize(),
                        listState = searchListState,
                        displayModules = uiState.searchResults,
                        updateInfoMap = uiState.updateInfo,
                        actions = actions,
                        onClickModule = { module ->
                            if (module.hasWebUi) {
                                actions.onOpenWebUi(module)
                                closeSearch()
                            }
                        },
                        onModuleAddShortcut = { module, type -> onModuleAddShortcut(module, type) },
                        closeSearch = closeSearch,
                    )
                }
            )
        },
        floatingActionButton = {
            if (uiState.installButtonVisible) {
                val moduleInstall = stringResource(id = R.string.module_install)
                val selectZipLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.StartActivityForResult()
                ) { activityResult ->
                    if (activityResult.resultCode != RESULT_OK) {
                        return@rememberLauncherForActivityResult
                    }
                    val data = activityResult.data ?: return@rememberLauncherForActivityResult
                    val clipData = data.clipData

                    val uris = mutableListOf<Uri>()
                    if (clipData != null) {
                        for (i in 0 until clipData.itemCount) {
                            clipData.getItemAt(i)?.uri?.let { uris.add(it) }
                        }
                    } else {
                        data.data?.let { uris.add(it) }
                    }

                    actions.onOpenFlash(uris)
                }

                ExtendedFloatingActionButton(
                    modifier = Modifier.padding(bottom = bottomInnerPadding),
                    expanded = fabExpanded,
                    onClick = {
                        // Select the zip files to install
                        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
                            type = "application/zip"
                            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                        }
                        selectZipLauncher.launch(intent)
                    },
                    icon = { Icon(Icons.Filled.Add, moduleInstall) },
                    text = { Text(text = moduleInstall) },
                )
            }
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        snackbarHost = { SnackbarHost(hostState = snackBarHost) }
    ) { innerPadding ->
        PullToRefreshBox(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding),
            isRefreshing = uiState.isRefreshing,
            onRefresh = {
                haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                actions.onRefresh()
            },
            state = pullToRefreshState,
            indicator = {
                PullToRefreshDefaults.LoadingIndicator(
                    modifier = Modifier.align(Alignment.TopCenter),
                    isRefreshing = uiState.isRefreshing,
                    state = pullToRefreshState,
                )
            },
        ) {
            if (uiState.magiskInstalled) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(24.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        stringResource(R.string.module_magisk_conflict),
                        textAlign = TextAlign.Center,
                    )
                }
                return@PullToRefreshBox
            }
            ModuleList(
                bottomInnerPadding = bottomInnerPadding,
                modifier = Modifier.nestedScroll(scrollBehavior.nestedScrollConnection),
                listState = listState,
                displayModules = uiState.moduleList,
                updateInfoMap = uiState.updateInfo,
                actions = actions,
                onClickModule = { module ->
                    if (module.hasWebUi) {
                        actions.onOpenWebUi(module)
                    }
                },
                onModuleAddShortcut = { module, type -> onModuleAddShortcut(module, type) },
            )
        }
    }

    ModuleShortcutSheet(
        show = showShortcutDialog.value,
        shortcutState = shortcutState,
        onDismiss = { showShortcutDialog.value = false },
        onPickShortcutIcon = { pickShortcutIconLauncher.launch("image/*") },
        onDeleteShortcut = {
            shortcutState.deleteShortcut(context)
            showShortcutDialog.value = false
        },
        onConfirmShortcut = {
            shortcutState.createShortcut(context)
            showShortcutDialog.value = false
        },
    )
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun ModuleList(
    bottomInnerPadding: Dp,
    modifier: Modifier = Modifier,
    listState: LazyListState = rememberLazyListState(),
    displayModules: List<Module>,
    updateInfoMap: Map<String, ModuleUpdateInfo>,
    actions: ModuleActions,
    onClickModule: (Module) -> Unit,
    onModuleAddShortcut: (Module, ShortcutType) -> Unit,
    closeSearch: () -> Unit? = {},
) {
    val loadingDialog = rememberLoadingDialog()
    LazyColumn(
        state = listState,
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
        contentPadding = PaddingValues(
            start = 16.dp,
            top = 8.dp,
            end = 16.dp,
            bottom = 16.dp + bottomInnerPadding + 56.dp + 16.dp
        ),
    ) {
        items(displayModules, key = { it.id }, contentType = { "module" }) { module ->
            val scope = rememberCoroutineScope()
            val moduleUpdateInfo = updateInfoMap[module.id] ?: ModuleUpdateInfo.Empty

            ModuleItem(
                module = module,
                updateUrl = moduleUpdateInfo.downloadUrl,
                onUninstallClicked = {
                    if (module.remove) {
                        actions.onUndoUninstallModule(module)
                    } else {
                        actions.onRequestUninstallConfirmation(module)
                    }
                },
                onCheckChanged = {
                    actions.onToggleModule(module)
                },
                onUpdate = {
                    scope.launch {
                        loadingDialog.withLoading {
                            actions.onRequestUpdateConfirmation(module, moduleUpdateInfo)
                        }
                    }
                },
                onAddShortcut = { type -> onModuleAddShortcut(module, type) },
                onClick = { onClickModule(module) },
                onExecuteAction = { actions.onExecuteModuleAction(module) },
                closeSearch = { closeSearch() }
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ModuleShortcutSheet(
    show: Boolean,
    shortcutState: ModuleShortcutState,
    onDismiss: () -> Unit,
    onPickShortcutIcon: () -> Unit,
    onDeleteShortcut: () -> Unit,
    onConfirmShortcut: () -> Unit,
) {
    if (!show) return

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(12.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp)
        ) {
            Text(
                text = stringResource(R.string.module_shortcut_title),
                style = MaterialTheme.typography.titleLarge
            )
            Box(
                contentAlignment = Alignment.Center,
                modifier = Modifier
                    .padding(vertical = 16.dp)
                    .size(100.dp)
                    .clip(RoundedCornerShape(25.dp))
            ) {
                val preview = shortcutState.previewIcon
                if (preview != null) {
                    Image(
                        bitmap = preview,
                        modifier = Modifier.size(100.dp),
                        contentDescription = null,
                    )
                } else {
                    Box(
                        modifier = Modifier
                            .size(100.dp)
                            .background(Color.White)
                    )
                    Image(
                        painter = painterResource(id = R.drawable.ic_launcher_foreground),
                        contentDescription = null,
                        contentScale = FixedScale(1.5f)
                    )
                }
            }
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically
            ) {
                TextButton(onClick = onPickShortcutIcon) {
                    Text(stringResource(id = R.string.module_shortcut_icon_pick))
                }
                AnimatedVisibility(
                    visible = shortcutState.iconUri != shortcutState.defaultShortcutIconUri,
                    enter = expandHorizontally() + slideInHorizontally(initialOffsetX = { it }),
                    exit = shrinkHorizontally() + slideOutHorizontally(targetOffsetX = { it }),
                ) {
                    IconButton(
                        onClick = shortcutState::resetIconToDefault,
                        modifier = Modifier.padding(start = 12.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Outlined.Refresh,
                            contentDescription = null,
                            modifier = Modifier.size(24.dp),
                        )
                    }
                }
            }
            OutlinedTextField(
                value = shortcutState.name,
                onValueChange = shortcutState::updateName,
                label = { Text(stringResource(id = R.string.module_shortcut_name_label)) },
                modifier = Modifier.fillMaxWidth()
            )
            if (shortcutState.hasExistingShortcut) {
                TextButton(
                    onClick = onDeleteShortcut,
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error)
                ) {
                    Text(stringResource(id = R.string.module_shortcut_delete))
                }
            }
            Row(
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                OutlinedButton(
                    onClick = onDismiss,
                    modifier = Modifier.weight(1f),
                ) {
                    Text(stringResource(id = android.R.string.cancel))
                }
                Button(
                    onClick = onConfirmShortcut,
                    modifier = Modifier.weight(1f),
                ) {
                    Text(
                        if (shortcutState.hasExistingShortcut) {
                            stringResource(id = R.string.module_update)
                        } else {
                            stringResource(id = android.R.string.ok)
                        }
                    )
                }
            }
            Spacer(modifier = Modifier.height(32.dp))
        }
    }
}

@OptIn(ExperimentalFoundationApi::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun ModuleItem(
    module: Module,
    updateUrl: String,
    onUninstallClicked: () -> Unit,
    onCheckChanged: (Boolean) -> Unit,
    onUpdate: () -> Unit,
    onAddShortcut: (ShortcutType) -> Unit,
    onClick: () -> Unit,
    onExecuteAction: () -> Unit,
    closeSearch: () -> Unit
) {
    TonalCard(
        modifier = Modifier.fillMaxWidth()
    ) {
        val haptic = LocalHapticFeedback.current
        val textDecoration = if (!module.remove) null else TextDecoration.LineThrough
        val interactionSource = remember { MutableInteractionSource() }
        val indication = LocalIndication.current
        var expanded by rememberSaveable(module.id) { mutableStateOf(false) }
        var isOverflowing by remember { mutableStateOf(false) }

        Column(
            modifier = Modifier
                .run {
                    if (module.hasWebUi) {
                        toggleable(
                            value = module.enabled,
                            enabled = !module.remove && module.enabled,
                            interactionSource = interactionSource,
                            role = Role.Button,
                            indication = indication,
                            onValueChange = { onClick() }
                        )
                    } else {
                        this
                    }
                }
                .padding(22.dp, 18.dp, 22.dp, 12.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                val moduleVersion = stringResource(id = R.string.module_version)
                val moduleAuthor = stringResource(id = R.string.module_author)

                Column(
                    modifier = Modifier.fillMaxWidth(0.8f)
                ) {
                    Text(
                        text = module.name,
                        fontWeight = FontWeight.SemiBold,
                        style = MaterialTheme.typography.titleMedium,
                        lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
                        textDecoration = textDecoration,
                    )

                    Text(
                        text = "$moduleVersion: ${module.version}",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        style = MaterialTheme.typography.bodySmall,
                        textDecoration = textDecoration
                    )

                    Text(
                        text = "$moduleAuthor: ${module.author}",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        style = MaterialTheme.typography.bodySmall,
                        textDecoration = textDecoration
                    )
                }

                Spacer(modifier = Modifier.weight(1f))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End,
                ) {
                    ExpressiveSwitch(
                        enabled = !module.update,
                        checked = module.enabled,
                        onCheckedChange = {
                            haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                            onCheckChanged(it)
                        },
                        interactionSource = if (!module.hasWebUi) interactionSource else remember { MutableInteractionSource() }
                    )
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            Text(
                modifier = Modifier
                    .animateContentSize(
                        animationSpec = tween(
                            durationMillis = 250,
                            easing = FastOutSlowInEasing
                        )
                    )
                    .then(
                        if (isOverflowing || expanded) {
                            Modifier.clickable(
                                interactionSource = remember { MutableInteractionSource() },
                                indication = null
                            ) { expanded = !expanded }
                        } else {
                            Modifier
                        }
                    ),
                text = module.description,
                color = MaterialTheme.colorScheme.outline,
                style = MaterialTheme.typography.bodyMedium,
                overflow = if (expanded) TextOverflow.Clip else TextOverflow.Ellipsis,
                maxLines = if (expanded) Int.MAX_VALUE else 4,
                textDecoration = textDecoration,
                onTextLayout = { textLayoutResult ->
                    isOverflowing = if (expanded) {
                        textLayoutResult.lineCount > 4
                    } else {
                        textLayoutResult.hasVisualOverflow
                    }
                }
            )

            Row(modifier = Modifier.padding(vertical = 4.dp)) {
                if (module.metamodule) {
                    StatusTag(
                        "META",
                        modifier = Modifier.padding(bottom = 4.dp),
                        contentColor = MaterialTheme.colorScheme.onPrimary,
                        backgroundColor = MaterialTheme.colorScheme.primary
                    )
                }
            }

            HorizontalDivider(thickness = Dp.Hairline)

            Spacer(modifier = Modifier.height(4.dp))

            Row(
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                val hasUpdate = updateUrl.isNotEmpty()
                val actionButtonsEnabled = !module.remove && module.enabled

                AnimatedVisibility(
                    visible = actionButtonsEnabled,
                    enter = fadeIn(),
                    exit = fadeOut()
                ) {
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        if (module.hasActionScript) {
                            CombinedClickableButton(
                                onClick = {
                                    onExecuteAction()
                                    closeSearch()
                                },
                                onLongClick = { onAddShortcut(ShortcutType.Action) },
                                modifier = Modifier.defaultMinSize(52.dp, 32.dp),
                                shape = ButtonDefaults.filledTonalShape,
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                    contentColor = MaterialTheme.colorScheme.onSecondaryContainer
                                ),
                                contentPadding = ButtonDefaults.TextButtonContentPadding
                            ) {
                                Icon(
                                    modifier = Modifier.size(20.dp),
                                    imageVector = Icons.Outlined.PlayArrow,
                                    contentDescription = null
                                )
                                if (!module.hasWebUi && !hasUpdate) {
                                    Text(
                                        modifier = Modifier.padding(start = 7.dp),
                                        text = stringResource(R.string.action),
                                        fontFamily = MaterialTheme.typography.labelMedium.fontFamily,
                                        fontSize = MaterialTheme.typography.labelMedium.fontSize
                                    )
                                }
                            }
                        }

                        if (module.hasWebUi) {
                            CombinedClickableButton(
                                onClick = {
                                    onClick()
                                    closeSearch()
                                },
                                onLongClick = { onAddShortcut(ShortcutType.WebUI) },
                                modifier = Modifier.defaultMinSize(52.dp, 32.dp),
                                shape = ButtonDefaults.filledTonalShape,
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                    contentColor = MaterialTheme.colorScheme.onSecondaryContainer
                                ),
                                contentPadding = ButtonDefaults.TextButtonContentPadding
                            ) {
                                Icon(
                                    modifier = Modifier.size(20.dp),
                                    imageVector = Icons.Outlined.Code,
                                    contentDescription = null
                                )
                                if (!module.hasActionScript && !hasUpdate) {
                                    Text(
                                        modifier = Modifier.padding(start = 7.dp),
                                        fontFamily = MaterialTheme.typography.labelMedium.fontFamily,
                                        fontSize = MaterialTheme.typography.labelMedium.fontSize,
                                        text = stringResource(R.string.open)
                                    )
                                }
                            }
                        }
                    }
                }

                Spacer(modifier = Modifier.weight(1f, true))

                AnimatedVisibility(
                    visible = hasUpdate,
                    enter = fadeIn(),
                    exit = fadeOut()
                ) {
                    Row {
                        Button(
                            modifier = Modifier.defaultMinSize(52.dp, 32.dp),
                            enabled = !module.remove,
                            onClick = onUpdate,
                            shape = ButtonDefaults.textShape,
                            contentPadding = ButtonDefaults.TextButtonContentPadding
                        ) {
                            Icon(
                                modifier = Modifier.size(20.dp),
                                imageVector = Icons.Outlined.Download,
                                contentDescription = null
                            )
                            if (!module.hasActionScript || !module.hasWebUi) {
                                Text(
                                    modifier = Modifier.padding(start = 7.dp),
                                    fontFamily = MaterialTheme.typography.labelMedium.fontFamily,
                                    fontSize = MaterialTheme.typography.labelMedium.fontSize,
                                    text = stringResource(R.string.module_update)
                                )
                            }
                        }

                        Spacer(Modifier.width(12.dp))
                    }
                }

                FilledTonalButton(
                    modifier = Modifier.defaultMinSize(52.dp, 32.dp),
                    onClick = onUninstallClicked,
                    contentPadding = ButtonDefaults.TextButtonContentPadding
                ) {
                    if (!module.remove) {
                        Icon(
                            modifier = Modifier.size(20.dp),
                            imageVector = Icons.Outlined.Delete,
                            contentDescription = null,
                        )
                    } else {
                        Icon(
                            modifier = Modifier
                                .size(20.dp)
                                .rotate(180f),
                            imageVector = Icons.Outlined.Refresh,
                            contentDescription = null,
                        )
                    }
                    if (!module.hasActionScript && !module.hasWebUi || !hasUpdate) {
                        Text(
                            modifier = Modifier.padding(start = 7.dp),
                            fontFamily = MaterialTheme.typography.labelMedium.fontFamily,
                            fontSize = MaterialTheme.typography.labelMedium.fontSize,
                            text = stringResource(if (module.remove) R.string.undo else R.string.uninstall)
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun CombinedClickableButton(
    onClick: () -> Unit,
    onLongClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    shape: Shape = ButtonDefaults.shape,
    colors: ButtonColors = ButtonDefaults.buttonColors(),
    border: BorderStroke? = null,
    contentPadding: PaddingValues = ButtonDefaults.ContentPadding,
    interactionSource: MutableInteractionSource? = null,
    content: @Composable RowScope.() -> Unit,
) {
    val interactionSource = interactionSource ?: remember { MutableInteractionSource() }

    Surface(
        modifier = modifier
            .semantics { role = Role.Button }
            .clip(shape)
            .combinedClickable(
                interactionSource = interactionSource,
                indication = LocalIndication.current,
                enabled = enabled,
                onClick = onClick,
                onLongClick = onLongClick
            ),
        shape = shape,
        color = if (enabled) colors.containerColor else colors.disabledContainerColor,
        contentColor = if (enabled) colors.contentColor else colors.disabledContentColor,
        border = border,
    ) {
        ProvideTextStyle(MaterialTheme.typography.labelLarge) {
            Row(
                Modifier
                    .defaultMinSize(
                        minWidth = ButtonDefaults.MinWidth,
                        minHeight = ButtonDefaults.MinHeight,
                    )
                    .padding(contentPadding),
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically,
                content = content,
            )
        }
    }
}
