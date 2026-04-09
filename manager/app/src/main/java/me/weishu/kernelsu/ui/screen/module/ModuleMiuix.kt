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
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandHorizontally
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkHorizontally
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.ime
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Code
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.FixedScale
import androidx.compose.ui.layout.SubcomposeLayout
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.data.model.ModuleUpdateInfo
import me.weishu.kernelsu.ui.component.ListPopupDefaults
import me.weishu.kernelsu.ui.component.SearchStatus
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.component.miuix.SearchBarFake
import me.weishu.kernelsu.ui.component.miuix.SearchBox
import me.weishu.kernelsu.ui.component.miuix.SearchPager
import me.weishu.kernelsu.ui.component.rebootlistpopup.RebootListPopupMiuix
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.BlurredBar
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.util.rememberBlurBackdrop
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.DropdownImpl
import top.yukonga.miuix.kmp.basic.FloatingActionButton
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.InfiniteProgressIndicator
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Switch
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import top.yukonga.miuix.kmp.blur.layerBackdrop
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Delete
import top.yukonga.miuix.kmp.icon.extended.Download
import top.yukonga.miuix.kmp.icon.extended.MoreCircle
import top.yukonga.miuix.kmp.icon.extended.Undo
import top.yukonga.miuix.kmp.icon.extended.UploadCloud
import top.yukonga.miuix.kmp.overlay.OverlayDialog
import top.yukonga.miuix.kmp.overlay.OverlayListPopup
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.theme.miuixShape
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@SuppressLint("StringFormatInvalid", "LocalContextGetResourceValueCall")
@Composable
fun ModulePagerMiuix(
    uiState: ModuleUiState,
    confirmDialogState: ModuleConfirmDialogState?,
    effect: ModuleEffect?,
    actions: ModuleActions,
    bottomInnerPadding: Dp,
) {
    val modules = uiState.moduleList
    val searchStatus = uiState.searchStatus

    val context = LocalContext.current
    val density = LocalDensity.current
    val enableBlur = LocalEnableBlur.current

    val installPromptWithName = stringResource(R.string.module_install_prompt_with_name, "%s")
    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            when (val request = confirmDialogState?.request) {
                is ModuleConfirmRequest.Uninstall -> {
                    actions.onUninstallModule(request.module)
                }

                is ModuleConfirmRequest.Update -> {
                    actions.onConfirmUpdate(request)
                }

                null -> Unit
            }
        },
        onDismiss = actions.onDismissConfirmRequest,
    )

    val scrollBehavior = MiuixScrollBehavior()
    var fabVisible by remember { mutableStateOf(true) }
    var scrollDistance by remember { mutableFloatStateOf(0f) }
    val dynamicTopPadding by remember {
        derivedStateOf { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }

    val shortcutState = rememberModuleShortcutState(context)
    val showShortcutDialog = remember { mutableStateOf(false) }

    val pickShortcutIconLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri ->
        shortcutState.updateIconUri(uri?.toString())
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
                Toast.makeText(context, effect.message, Toast.LENGTH_SHORT).show()
                actions.onConsumeEffect()
            }

            null -> Unit
        }
    }

    fun onModuleAddShortcut(module: Module, type: ShortcutType) {
        shortcutState.bindModule(module)
        shortcutState.selectType(type)
        showShortcutDialog.value = true
    }

    val listState = rememberLazyListState()
    val nestedScrollConnection = remember(uiState.installButtonVisible) {
        object : NestedScrollConnection {
            override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
                val isScrolledToEnd =
                    (listState.layoutInfo.visibleItemsInfo.lastOrNull()?.index == listState.layoutInfo.totalItemsCount - 1
                            && (listState.layoutInfo.visibleItemsInfo.lastOrNull()?.size
                        ?: 0) < listState.layoutInfo.viewportEndOffset)
                val delta = available.y
                if (!isScrolledToEnd) {
                    scrollDistance += delta
                    if (scrollDistance < -50f) {
                        if (fabVisible) fabVisible = false
                        scrollDistance = 0f
                    } else if (scrollDistance > 50f) {
                        if (!fabVisible) fabVisible = true
                        scrollDistance = 0f
                    }
                }
                return Offset.Zero
            }
        }
    }
    val offsetHeight by animateDpAsState(
        targetValue = if (fabVisible) 0.dp else 180.dp + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding(),
        animationSpec = tween(durationMillis = 350)
    )

    val backdrop = rememberBlurBackdrop(enableBlur)
    val blurActive = backdrop != null
    val barColor = if (blurActive) Color.Transparent else colorScheme.surface

    Scaffold(
        topBar = {
            BlurredBar(backdrop) {
                searchStatus.TopAppBarAnim(backgroundColor = barColor) {
                    TopAppBar(
                        color = barColor,
                        title = stringResource(R.string.module),
                        actions = {
                            Box {
                                val showTopPopup = remember { mutableStateOf(false) }
                                IconButton(
                                    onClick = { showTopPopup.value = true },
                                    holdDownState = showTopPopup.value
                                ) {
                                    Icon(
                                        imageVector = MiuixIcons.MoreCircle,
                                        tint = colorScheme.onSurface,
                                        contentDescription = null
                                    )
                                }
                                OverlayListPopup(
                                    show = showTopPopup.value,
                                    popupPositionProvider = ListPopupDefaults.MenuPositionProvider,
                                    alignment = PopupPositionProvider.Align.TopEnd,
                                    onDismissRequest = {
                                        showTopPopup.value = false
                                    },
                                    content = {
                                        ListPopupColumn {
                                            DropdownImpl(
                                                text = stringResource(R.string.module_sort_action_first),
                                                optionSize = 2,
                                                isSelected = uiState.sortActionFirst,
                                                onSelectedIndexChange = {
                                                    actions.onToggleSortActionFirst()
                                                    showTopPopup.value = false
                                                },
                                                index = 0
                                            )
                                            DropdownImpl(
                                                text = stringResource(R.string.module_sort_enabled_first),
                                                optionSize = 2,
                                                isSelected = uiState.sortEnabledFirst,
                                                onSelectedIndexChange = {
                                                    actions.onToggleSortEnabledFirst()
                                                    showTopPopup.value = false
                                                },
                                                index = 1
                                            )
                                        }
                                    }
                                )
                            }
                            RebootListPopupMiuix(
                                alignment = PopupPositionProvider.Align.TopEnd,
                            )
                        },
                        navigationIcon = {
                            IconButton(
                                onClick = actions.onOpenRepo,
                            ) {
                                Icon(
                                    imageVector = MiuixIcons.Download,
                                    tint = colorScheme.onSurface,
                                    contentDescription = null
                                )
                            }
                        },
                        scrollBehavior = scrollBehavior,
                        bottomContent = {
                            Box(
                                modifier = Modifier
                                    .alpha(if (searchStatus.isCollapsed()) 1f else 0f)
                                    .onGloballyPositioned { coordinates ->
                                        with(density) {
                                            val newOffsetY = coordinates.positionInWindow().y.toDp()
                                            if (searchStatus.offsetY != newOffsetY) {
                                                actions.onSearchStatusChange(searchStatus.copy(offsetY = newOffsetY))
                                            }
                                        }
                                    }
                                    .then(
                                        if (searchStatus.isCollapsed()) {
                                            Modifier.pointerInput(Unit) {
                                                detectTapGestures {
                                                    actions.onSearchStatusChange(searchStatus.copy(current = SearchStatus.Status.EXPANDING))
                                                }
                                            }
                                        } else Modifier,
                                    ),
                            ) {
                                SearchBarFake(searchStatus.label, dynamicTopPadding)
                            }
                        }
                    )
                }
            }
        },
        floatingActionButton = {
            if (uiState.installButtonVisible) {
                val moduleInstall = stringResource(id = R.string.module_install)
                val confirmTitle = stringResource(R.string.module)
                var zipUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
                val confirmDialog = rememberConfirmDialog(
                    onConfirm = {
                        actions.onOpenFlash(zipUris)
                    }
                )
                val selectZipLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.StartActivityForResult()
                ) { activityResult ->
                    val uris = mutableListOf<Uri>()
                    if (activityResult.resultCode != RESULT_OK) {
                        return@rememberLauncherForActivityResult
                    }
                    val data = activityResult.data ?: return@rememberLauncherForActivityResult
                    val clipData = data.clipData

                    if (clipData != null) {
                        for (i in 0 until clipData.itemCount) {
                            clipData.getItemAt(i)?.uri?.let { uris.add(it) }
                        }
                    } else {
                        data.data?.let { uris.add(it) }
                    }

                    if (uris.size == 1) {
                        actions.onOpenFlash(listOf(uris.first()))
                    } else if (uris.size > 1) {
                        // multiple files selected
                        zipUris = uris
                        val moduleNames = uris.mapIndexed { index, uri -> "\n${index + 1}. ${uri.getFileName(context)}" }.joinToString("")
                        val confirmContent = installPromptWithName.format(moduleNames)
                        confirmDialog.showConfirm(
                            title = confirmTitle,
                            content = confirmContent
                        )
                    }
                }
                FloatingActionButton(
                    modifier = Modifier
                        .offset {
                            IntOffset(x = 0, y = offsetHeight.roundToPx())
                        }
                        .padding(bottom = bottomInnerPadding + 20.dp, end = 20.dp)
                        .border(0.05.dp, colorScheme.outline.copy(alpha = 0.5f), CircleShape),
                    shadowElevation = 0.dp,
                    onClick = {
                        // Select the zip files to install
                        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
                            type = "application/zip"
                            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                        }
                        selectZipLauncher.launch(intent)
                    },
                    content = {
                        Icon(
                            Icons.Rounded.Add,
                            moduleInstall,
                            modifier = Modifier.size(40.dp),
                            tint = colorScheme.onPrimary
                        )
                    },
                )
            }
        },
        popupHost = {
            searchStatus.SearchPager(
                onSearchStatusChange = actions.onSearchStatusChange,
                defaultResult = {},
                searchBarTopPadding = dynamicTopPadding,
            ) {
                val imeBottomPadding = WindowInsets.ime.asPaddingValues().calculateBottomPadding()
                ModuleList(
                    modifier = Modifier
                        .fillMaxSize()
                        .overScrollVertical(),
                    modules = uiState.searchResults,
                    updateInfoMap = uiState.updateInfo,
                    actions = actions,
                    onModuleAddShortcut = ::onModuleAddShortcut,
                    contentPadding = PaddingValues(
                        top = 6.dp,
                        start = 0.dp,
                        end = 0.dp,
                        bottom = maxOf(bottomInnerPadding, imeBottomPadding),
                    ),
                )
            }
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        if (uiState.magiskInstalled) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(12.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    stringResource(R.string.module_magisk_conflict),
                    textAlign = TextAlign.Center,
                )
            }
            return@Scaffold
        }
        val layoutDirection = LocalLayoutDirection.current
        val pullToRefreshState = rememberPullToRefreshState()
        val refreshTexts = listOf(
            stringResource(R.string.refresh_pulling),
            stringResource(R.string.refresh_release),
            stringResource(R.string.refresh_refresh),
            stringResource(R.string.refresh_complete),
        )
        searchStatus.SearchBox(
            onSearchStatusChange = actions.onSearchStatusChange,
        ) {
            val contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding() + 6.dp,
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection),
                bottom = bottomInnerPadding,
            )
            if (modules.isEmpty() && !uiState.hasLoaded) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(
                            top = innerPadding.calculateTopPadding(),
                            start = innerPadding.calculateStartPadding(layoutDirection),
                            end = innerPadding.calculateEndPadding(layoutDirection),
                            bottom = bottomInnerPadding
                        ),
                    contentAlignment = Alignment.Center
                ) {
                    InfiniteProgressIndicator()
                }
            } else {
                PullToRefresh(
                    isRefreshing = uiState.isRefreshing,
                    pullToRefreshState = pullToRefreshState,
                    onRefresh = actions.onRefresh,
                    refreshTexts = refreshTexts,
                    contentPadding = contentPadding,
                ) {
                    if (modules.isEmpty()) {
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .padding(
                                    top = innerPadding.calculateTopPadding(),
                                    start = innerPadding.calculateStartPadding(layoutDirection),
                                    end = innerPadding.calculateEndPadding(layoutDirection),
                                    bottom = bottomInnerPadding
                                ),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                stringResource(R.string.module_empty),
                                textAlign = TextAlign.Center,
                                color = Color.Gray,
                            )
                        }
                    } else {
                        Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
                            ModuleList(
                                modifier = Modifier
                                    .fillMaxHeight()
                                    .scrollEndHaptic()
                                    .overScrollVertical()
                                    .nestedScroll(scrollBehavior.nestedScrollConnection)
                                    .nestedScroll(nestedScrollConnection),
                                modules = modules,
                                updateInfoMap = uiState.updateInfo,
                                actions = actions,
                                onModuleAddShortcut = { module, type ->
                                    onModuleAddShortcut(module, type)
                                },
                                contentPadding = contentPadding,
                            )
                        }
                    }
                }
            }
        }
    }
    ModuleShortcutDialog(
        show = showShortcutDialog.value,
        onDismissRequest = { showShortcutDialog.value = false },
        shortcutState = shortcutState,
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

@Composable
private fun ModuleShortcutDialog(
    show: Boolean,
    onDismissRequest: () -> Unit,
    shortcutState: ModuleShortcutState,
    onPickShortcutIcon: () -> Unit,
    onDeleteShortcut: () -> Unit,
    onConfirmShortcut: () -> Unit,
) {
    OverlayDialog(
        show = show,
        title = stringResource(R.string.module_shortcut_title),
        onDismissRequest = onDismissRequest,
        content = {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .padding(vertical = 16.dp)
                        .size(100.dp)
                        .clip(miuixShape(25.dp))
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
                Row {
                    TextButton(
                        modifier = Modifier.weight(1f),
                        text = stringResource(id = R.string.module_shortcut_icon_pick),
                        onClick = onPickShortcutIcon,
                    )
                    AnimatedVisibility(
                        visible = shortcutState.iconUri != shortcutState.defaultShortcutIconUri,
                        enter = expandHorizontally() + slideInHorizontally(initialOffsetX = { it }),
                        exit = shrinkHorizontally() + slideOutHorizontally(targetOffsetX = { it }),
                        modifier = Modifier.align(Alignment.CenterVertically),
                    ) {
                        IconButton(
                            onClick = shortcutState::resetIconToDefault,
                            modifier = Modifier.padding(start = 12.dp)
                        ) {
                            Icon(
                                imageVector = MiuixIcons.Undo,
                                contentDescription = null,
                                tint = colorScheme.onSurface,
                                modifier = Modifier.size(28.dp),
                            )
                        }
                    }
                }
                TextField(
                    value = shortcutState.name,
                    onValueChange = shortcutState::updateName,
                    label = stringResource(id = R.string.module_shortcut_name_label)
                )
                if (shortcutState.hasExistingShortcut) {
                    TextButton(
                        text = stringResource(id = R.string.module_shortcut_delete),
                        onClick = onDeleteShortcut,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                Row(
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    TextButton(
                        text = stringResource(id = android.R.string.cancel),
                        onClick = onDismissRequest,
                        modifier = Modifier.weight(1f),
                    )
                    TextButton(
                        text = if (shortcutState.hasExistingShortcut) {
                            stringResource(id = R.string.module_update)
                        } else {
                            stringResource(id = android.R.string.ok)
                        },
                        onClick = onConfirmShortcut,
                        colors = ButtonDefaults.textButtonColorsPrimary(),
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        }
    )
}

@Composable
private fun ModuleList(
    modifier: Modifier = Modifier,
    modules: List<Module>,
    updateInfoMap: Map<String, ModuleUpdateInfo>,
    actions: ModuleActions,
    onModuleAddShortcut: (Module, ShortcutType) -> Unit,
    contentPadding: PaddingValues,
) {
    val loadingDialog = rememberLoadingDialog()
    val scope = rememberCoroutineScope()
    LazyColumn(
        modifier = modifier.fillMaxHeight(),
        contentPadding = contentPadding,
        overscrollEffect = null,
    ) {
        items(
            items = modules,
            key = { it.id },
            contentType = { "module" }
        ) { module ->
            val currentModuleState = rememberUpdatedState(module)
            val moduleUpdateInfo = updateInfoMap[module.id] ?: ModuleUpdateInfo.Empty
            val content: @Composable () -> Unit = {
                ModuleItem(
                    module = module,
                    updateUrl = moduleUpdateInfo.downloadUrl,
                    onUninstall = {
                        actions.onRequestUninstallConfirmation(currentModuleState.value)
                    },
                    onUndoUninstall = {
                        scope.launch {
                            loadingDialog.withLoading { actions.onUndoUninstallModule(module) }
                        }
                    },
                    onCheckChanged = { _: Boolean ->
                        scope.launch {
                            loadingDialog.withLoading {
                                actions.onToggleModule(module)
                            }
                        }
                    },
                    onUpdate = {
                        scope.launch {
                            loadingDialog.withLoading {
                                actions.onRequestUpdateConfirmation(currentModuleState.value, moduleUpdateInfo)
                            }
                        }
                    },
                    onExecuteAction = {
                        actions.onExecuteModuleAction(currentModuleState.value)
                    },
                    onAddActionShortcut = { type: ShortcutType ->
                        onModuleAddShortcut(currentModuleState.value, type)
                    },
                    onOpenWebUi = {
                        if (module.hasWebUi) {
                            actions.onOpenWebUi(module)
                        }
                    }
                )
            }

            content()
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun ModuleItem(
    module: Module,
    updateUrl: String,
    onUndoUninstall: () -> Unit,
    onUninstall: () -> Unit,
    onCheckChanged: (Boolean) -> Unit,
    onUpdate: () -> Unit,
    onExecuteAction: () -> Unit,
    onAddActionShortcut: (ShortcutType) -> Unit,
    onOpenWebUi: () -> Unit
) {
    val secondaryContainer = colorScheme.secondaryContainer.copy(alpha = 0.8f)
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isInDarkTheme()) 0.7f else 0.9f)
    val updateBg = colorScheme.tertiaryContainer.copy(alpha = 0.6f)
    val updateTint = colorScheme.onTertiaryContainer.copy(alpha = 0.8f)
    val hasUpdate = updateUrl.isNotEmpty()
    val textDecoration = if (module.remove) TextDecoration.LineThrough else null
    val hasDescription = module.description.isNotBlank()
    var expanded by rememberSaveable(module.id) { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .padding(horizontal = 12.dp)
            .padding(bottom = 12.dp),
        insideMargin = PaddingValues(16.dp),
        onClick = {
            if (hasDescription) expanded = !expanded
        }
    ) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(end = 4.dp)
            ) {
                val moduleVersion = stringResource(id = R.string.module_version)
                val moduleAuthor = stringResource(id = R.string.module_author)

                SubcomposeLayout { constraints ->
                    val spacingPx = 6.dp.roundToPx()
                    var nameTextLayout: TextLayoutResult? = null
                    val metaPlaceable = if (module.metamodule) {
                        subcompose("meta") {
                            Text(
                                text = "META",
                                fontSize = 12.sp,
                                color = updateTint,
                                modifier = Modifier
                                    .clip(miuixShape(6.dp))
                                    .background(updateBg)
                                    .padding(horizontal = 6.dp, vertical = 2.dp),
                                fontWeight = FontWeight(750),
                                maxLines = 1,
                                softWrap = false
                            )
                        }.first().measure(Constraints(0, constraints.maxWidth, 0, constraints.maxHeight))
                    } else null

                    val reserved = (metaPlaceable?.width ?: 0) + if (metaPlaceable != null) spacingPx else 0
                    val nameMax = (constraints.maxWidth - reserved).coerceAtLeast(0)
                    val namePlaceable = subcompose("name") {
                        Text(
                            text = module.name,
                            fontSize = 17.sp,
                            fontWeight = FontWeight(550),
                            color = colorScheme.onSurface,
                            textDecoration = textDecoration,
                            onTextLayout = { nameTextLayout = it }
                        )
                    }.first().measure(Constraints(constraints.minWidth, nameMax, constraints.minHeight, constraints.maxHeight))

                    val width = (namePlaceable.width + reserved).coerceIn(constraints.minWidth, constraints.maxWidth)
                    val height = maxOf(namePlaceable.height, metaPlaceable?.height ?: 0)

                    layout(width, height) {
                        namePlaceable.placeRelative(0, 0)
                        val endX = nameTextLayout?.let { layoutRes ->
                            val last = (layoutRes.lineCount - 1).coerceAtLeast(0)
                            layoutRes.getLineRight(last).toInt()
                        } ?: namePlaceable.width
                        metaPlaceable?.placeRelative(endX + spacingPx, (height - (metaPlaceable.height)) / 2)
                    }
                }
                Text(
                    text = "$moduleVersion: ${module.version}",
                    fontSize = 12.sp,
                    modifier = Modifier.padding(top = 2.dp),
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurfaceVariantSummary,
                    textDecoration = textDecoration
                )
                Text(
                    text = "$moduleAuthor: ${module.author}",
                    fontSize = 12.sp,
                    modifier = Modifier.padding(bottom = 1.dp),
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurfaceVariantSummary,
                    textDecoration = textDecoration
                )
            }
            Switch(
                enabled = !module.update,
                checked = module.enabled,
                onCheckedChange = {
                    if (it != module.enabled) onCheckChanged(it)
                }
            )
        }

        if (hasDescription) {
            Box(
                modifier = Modifier
                    .padding(top = 2.dp)
                    .animateContentSize(
                        animationSpec = tween(
                            durationMillis = 250,
                            easing = FastOutSlowInEasing
                        )
                    )
            ) {
                Text(
                    text = module.description,
                    fontSize = 14.sp,
                    color = colorScheme.onSurfaceVariantSummary,
                    overflow = if (expanded) TextOverflow.Clip else TextOverflow.Ellipsis,
                    maxLines = if (expanded) Int.MAX_VALUE else 4,
                    textDecoration = textDecoration
                )
            }
        }

        HorizontalDivider(
            modifier = Modifier.padding(vertical = 8.dp),
            thickness = 0.5.dp,
            color = colorScheme.outline.copy(alpha = 0.5f)
        )

        Row {
            AnimatedVisibility(
                visible = module.enabled && !module.remove && !module.update,
                enter = fadeIn(),
                exit = fadeOut()
            ) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (module.hasActionScript) {
                        Row(
                            modifier = Modifier
                                .heightIn(min = 35.dp)
                                .widthIn(min = 35.dp)
                                .clip(CircleShape)
                                .background(secondaryContainer)
                                .combinedClickable(
                                    onClick = onExecuteAction,
                                    onLongClick = { onAddActionShortcut(ShortcutType.Action) }
                                )
                                .padding(
                                    start = if (!module.hasWebUi && !hasUpdate) 6.dp else 0.dp,
                                    end = if (!module.hasWebUi && !hasUpdate) 8.dp else 0.dp,
                                ),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.Center
                        ) {
                            Icon(
                                modifier = Modifier.size(24.dp),
                                imageVector = Icons.Rounded.PlayArrow,
                                tint = actionIconTint,
                                contentDescription = stringResource(R.string.action)
                            )
                            if (!module.hasWebUi && !hasUpdate) {
                                Text(
                                    modifier = Modifier.padding(start = 3.dp, end = 4.dp),
                                    text = stringResource(R.string.action),
                                    color = actionIconTint,
                                    fontWeight = FontWeight.Medium,
                                    fontSize = 15.sp,
                                )
                            }
                        }
                    }
                    if (module.hasWebUi) {
                        Row(
                            modifier = Modifier
                                .heightIn(min = 35.dp)
                                .widthIn(min = 35.dp)
                                .clip(CircleShape)
                                .background(secondaryContainer)
                                .combinedClickable(
                                    onClick = onOpenWebUi,
                                    onLongClick = { onAddActionShortcut(ShortcutType.WebUI) }
                                )
                                .padding(horizontal = if (!module.hasActionScript && !hasUpdate) 10.dp else 0.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.Center
                        ) {
                            Icon(
                                modifier = Modifier.size(22.dp),
                                imageVector = Icons.Rounded.Code,
                                tint = actionIconTint,
                                contentDescription = stringResource(R.string.open)
                            )
                            if (!module.hasActionScript && !hasUpdate) {
                                Text(
                                    modifier = Modifier.padding(start = 4.dp, end = 2.dp),
                                    text = stringResource(R.string.open),
                                    color = actionIconTint,
                                    fontWeight = FontWeight.Medium,
                                    fontSize = 15.sp,
                                )
                            }
                        }
                    }
                }
            }

            Spacer(Modifier.weight(1f))

            AnimatedVisibility(
                visible = hasUpdate,
                enter = fadeIn(),
                exit = fadeOut()
            ) {
                IconButton(
                    backgroundColor = updateBg,
                    enabled = !module.remove,
                    minHeight = 35.dp,
                    minWidth = 35.dp,
                    onClick = onUpdate,
                ) {
                    Row(
                        modifier = Modifier.padding(horizontal = 10.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(2.dp),
                    ) {
                        Icon(
                            modifier = Modifier.size(20.dp),
                            imageVector = MiuixIcons.UploadCloud,
                            tint = updateTint,
                            contentDescription = stringResource(R.string.module_update),
                        )
                        Text(
                            modifier = Modifier.padding(start = 4.dp, end = 3.dp),
                            text = stringResource(R.string.module_update),
                            color = updateTint,
                            fontWeight = FontWeight.Medium,
                            fontSize = 15.sp
                        )
                    }
                }
            }
            IconButton(
                minHeight = 35.dp,
                minWidth = 35.dp,
                onClick = if (module.remove) onUndoUninstall else onUninstall,
                backgroundColor = if (module.remove) {
                    secondaryContainer.copy(alpha = 0.8f)
                } else {
                    secondaryContainer
                },
            ) {
                val animatedPadding by animateDpAsState(
                    targetValue = if (!hasUpdate) 10.dp else 0.dp,
                    animationSpec = tween(durationMillis = 300)
                )
                Row(
                    modifier = Modifier.padding(horizontal = animatedPadding),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(
                        modifier = Modifier.size(20.dp),
                        imageVector = if (module.remove) {
                            MiuixIcons.Undo
                        } else {
                            MiuixIcons.Delete
                        },
                        tint = actionIconTint,
                        contentDescription = null
                    )
                    AnimatedVisibility(
                        visible = !hasUpdate,
                        enter = expandHorizontally(),
                        exit = shrinkHorizontally()
                    ) {
                        Text(
                            modifier = Modifier.padding(start = 4.dp, end = 3.dp),
                            text = stringResource(
                                if (module.remove) R.string.undo else R.string.uninstall
                            ),
                            color = actionIconTint,
                            fontWeight = FontWeight.Medium,
                            fontSize = 15.sp
                        )
                    }
                }
            }
        }
    }
}
