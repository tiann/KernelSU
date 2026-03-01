package me.weishu.kernelsu.ui.screen

import android.annotation.SuppressLint
import android.app.Activity.RESULT_OK
import android.content.Context
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
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkHorizontally
import androidx.compose.animation.shrinkVertically
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.ime
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
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
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.layout.FixedScale
import androidx.compose.ui.layout.SubcomposeLayout
import androidx.compose.ui.platform.LocalContext
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.edit
import androidx.core.net.toUri
import androidx.lifecycle.viewmodel.compose.viewModel
import com.kyant.capsule.ContinuousRoundedRectangle
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.ConfirmResult
import me.weishu.kernelsu.ui.component.ListPopupDefaults.MenuPositionProvider
import me.weishu.kernelsu.ui.component.RebootListPopup
import me.weishu.kernelsu.ui.component.SearchBox
import me.weishu.kernelsu.ui.component.SearchPager
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rememberLoadingDialog
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.util.hasMagisk
import me.weishu.kernelsu.ui.util.module.Shortcut
import me.weishu.kernelsu.ui.util.module.fetchModuleDetail
import me.weishu.kernelsu.ui.util.module.fetchReleaseDescriptionHtml
import me.weishu.kernelsu.ui.util.toggleModule
import me.weishu.kernelsu.ui.util.undoUninstallModule
import me.weishu.kernelsu.ui.util.uninstallModule
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import me.weishu.kernelsu.ui.webui.WebUIActivity
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.DropdownImpl
import top.yukonga.miuix.kmp.basic.FloatingActionButton
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
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
import top.yukonga.miuix.kmp.extra.SuperDialog
import top.yukonga.miuix.kmp.extra.SuperListPopup
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Delete
import top.yukonga.miuix.kmp.icon.extended.Download
import top.yukonga.miuix.kmp.icon.extended.MoreCircle
import top.yukonga.miuix.kmp.icon.extended.Undo
import top.yukonga.miuix.kmp.icon.extended.UploadCloud
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

private enum class ShortcutType {
    Action,
    WebUI
}

@SuppressLint("LocalContextGetResourceValueCall", "StringFormatInvalid")
@Composable
fun ModulePager(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    val viewModel = viewModel<ModuleViewModel>()
    val uiState by viewModel.uiState.collectAsState()
    val modules = uiState.moduleList
    val scope = rememberCoroutineScope()
    val searchStatus = uiState.searchStatus

    val context = LocalContext.current
    var isInitialized by rememberSaveable { mutableStateOf(false) }
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val enableBlur = LocalEnableBlur.current

    LaunchedEffect(Unit) {
        when {
            !isInitialized || modules.isEmpty() -> {
                viewModel.setCheckModuleUpdate(prefs.getBoolean("module_check_update", true))
                viewModel.setSortEnabledFirst(prefs.getBoolean("module_sort_enabled_first", false))
                viewModel.setSortActionFirst(prefs.getBoolean("module_sort_action_first", false))
                viewModel.fetchModuleList(checkUpdate = true)
                isInitialized = true
            }

            viewModel.isNeedRefresh -> {
                viewModel.fetchModuleList(checkUpdate = true)
            }
        }
    }

    LaunchedEffect(searchStatus.searchText) {
        viewModel.updateSearchText(searchStatus.searchText)
    }

    LaunchedEffect(modules) {
        if (searchStatus.searchText.isNotEmpty()) {
            viewModel.updateSearchText(searchStatus.searchText)
        }
    }

    val webUILauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { viewModel.fetchModuleList() }

    val loadingDialog = rememberLoadingDialog()
    val confirmDialog = rememberConfirmDialog()

    val isSafeMode = Natives.isSafeMode
    val magiskInstalled by produceState(initialValue = false) {
        value = withContext(Dispatchers.IO) { hasMagisk() }
    }
    val hideInstallButton = isSafeMode || magiskInstalled

    val scrollBehavior = MiuixScrollBehavior()
    var fabVisible by remember { mutableStateOf(true) }
    var scrollDistance by remember { mutableFloatStateOf(0f) }
    val dynamicTopPadding by remember {
        derivedStateOf { 12.dp * (1f - scrollBehavior.state.collapsedFraction) }
    }

    val failedEnable = stringResource(R.string.module_failed_to_enable)
    val failedDisable = stringResource(R.string.module_failed_to_disable)
    val failedUndoUninstall = stringResource(R.string.module_undo_uninstall_failed)
    val successUndoUninstall = stringResource(R.string.module_undo_uninstall_success)
    val failedUninstall = stringResource(R.string.module_uninstall_failed)
    val successUninstall = stringResource(R.string.module_uninstall_success)
    val rebootToApply = stringResource(R.string.reboot_to_apply)
    val moduleStr = stringResource(R.string.module)
    val uninstall = stringResource(R.string.uninstall)
    val cancel = stringResource(android.R.string.cancel)
    val moduleUninstallConfirm = stringResource(R.string.module_uninstall_confirm)
    val metaModuleUninstallConfirm = stringResource(R.string.metamodule_uninstall_confirm)
    val updateText = stringResource(R.string.module_update)
    val changelogText = stringResource(R.string.module_changelog)
    val downloadingText = stringResource(R.string.module_downloading)
    val startDownloadingText = stringResource(R.string.module_start_downloading)

    var shortcutModuleId by rememberSaveable { mutableStateOf<String?>(null) }
    var shortcutName by rememberSaveable { mutableStateOf("") }
    var shortcutIconUri by rememberSaveable { mutableStateOf<String?>(null) }
    var defaultShortcutIconUri by rememberSaveable { mutableStateOf<String?>(null) }
    var defaultActionShortcutIconUri by rememberSaveable { mutableStateOf<String?>(null) }
    var defaultWebUiShortcutIconUri by rememberSaveable { mutableStateOf<String?>(null) }
    var selectedShortcutType by rememberSaveable { mutableStateOf<ShortcutType?>(null) }
    val showShortcutDialog = remember { mutableStateOf(false) }
    val showShortcutTypeDialog = remember { mutableStateOf(false) }

    fun openShortcutDialogForType(type: ShortcutType) {
        selectedShortcutType = type
        val defaultIcon = when (type) {
            ShortcutType.Action -> defaultActionShortcutIconUri ?: defaultWebUiShortcutIconUri
            ShortcutType.WebUI -> defaultWebUiShortcutIconUri ?: defaultActionShortcutIconUri
        }
        defaultShortcutIconUri = defaultIcon
        shortcutIconUri = defaultIcon
        showShortcutDialog.value = true
    }

    fun hasModuleShortcut(context: Context, moduleId: String, type: ShortcutType): Boolean {
        return when (type) {
            ShortcutType.Action -> Shortcut.hasModuleActionShortcut(context, moduleId)
            ShortcutType.WebUI -> Shortcut.hasModuleWebUiShortcut(context, moduleId)
        }
    }

    fun deleteModuleShortcut(context: Context, moduleId: String, type: ShortcutType) {
        when (type) {
            ShortcutType.Action -> Shortcut.deleteModuleActionShortcut(context, moduleId)
            ShortcutType.WebUI -> Shortcut.deleteModuleWebUiShortcut(context, moduleId)
        }
    }

    fun createModuleShortcut(
        context: Context,
        moduleId: String,
        name: String,
        iconUri: String?,
        type: ShortcutType
    ) {
        when (type) {
            ShortcutType.Action -> {
                Shortcut.createModuleActionShortcut(
                    context = context,
                    moduleId = moduleId,
                    name = name,
                    iconUri = iconUri
                )
            }

            ShortcutType.WebUI -> {
                Shortcut.createModuleWebUiShortcut(
                    context = context,
                    moduleId = moduleId,
                    name = name,
                    iconUri = iconUri
                )
            }
        }
    }

    val pickShortcutIconLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri ->
        shortcutIconUri = uri?.toString()
    }

    val shortcutPreviewIcon = remember { mutableStateOf<ImageBitmap?>(null) }
    LaunchedEffect(shortcutIconUri) {
        val uriStr = shortcutIconUri
        if (uriStr.isNullOrBlank()) {
            shortcutPreviewIcon.value = null
            return@LaunchedEffect
        }
        val bitmap = withContext(Dispatchers.IO) {
            Shortcut.loadShortcutBitmap(context, uriStr)
        }
        shortcutPreviewIcon.value = bitmap?.asImageBitmap()
    }

    var hasExistingShortcut by rememberSaveable { mutableStateOf(false) }
    LaunchedEffect(shortcutModuleId, selectedShortcutType, showShortcutDialog.value) {
        val moduleId = shortcutModuleId
        val type = selectedShortcutType
        if (!showShortcutDialog.value || moduleId.isNullOrBlank() || type == null) {
            hasExistingShortcut = false
            return@LaunchedEffect
        }
        val exists = withContext(Dispatchers.IO) {
            hasModuleShortcut(context, moduleId, type)
        }
        hasExistingShortcut = exists
    }

    suspend fun onModuleUpdate(
        module: ModuleViewModel.ModuleInfo,
        changelogUrl: String,
        downloadUrl: String,
        fileName: String,
        context: Context,
        onInstallModule: (Uri) -> Unit
    ) {
        val changelogResult = if (changelogUrl.isBlank()) {
            Result.success("")
        } else {
            loadingDialog.withLoading {
                withContext(Dispatchers.IO) {
                    runCatching {
                        ksuApp.okhttpClient.newCall(
                            okhttp3.Request.Builder().url(changelogUrl).build()
                        ).execute().body.string()
                    }
                }
            }
        }

        val showToast: suspend (String) -> Unit = { msg ->
            withContext(Dispatchers.Main) {
                Toast.makeText(
                    context,
                    msg,
                    Toast.LENGTH_SHORT
                ).show()
            }
        }

        val changelog = changelogResult.getOrElse { "" }
        var htmlLog = ""
        if (changelog.isBlank()) {
            withContext(Dispatchers.IO) {
                runCatching {
                    val detail = fetchModuleDetail(module.id)
                    val latestTag = detail?.latestTag ?: ""
                    val html = if (latestTag.isNotBlank()) fetchReleaseDescriptionHtml(module.id, latestTag) else null
                    if (html != null) htmlLog = html
                }
            }
        }

        val confirmResult = confirmDialog.awaitConfirm(
            if (changelog.isNotEmpty() || htmlLog.isNotEmpty()) changelogText else updateText,
            content = when {
                changelog.isNotEmpty() -> changelog
                htmlLog.isNotEmpty() -> htmlLog
                else -> startDownloadingText.format(module.name)
            },
            markdown = changelog.isNotEmpty(),
            html = htmlLog.isNotEmpty(),
            confirm = updateText,
        )

        if (confirmResult != ConfirmResult.Confirmed) {
            return
        }

        showToast(startDownloadingText.format(module.name))

        val downloading = downloadingText.format(module.name)
        withContext(Dispatchers.IO) {
            download(
                url = downloadUrl,
                fileName = fileName,
                onDownloaded = onInstallModule,
                onDownloading = {
                    scope.launch(Dispatchers.Main) {
                        Toast.makeText(context, downloading, Toast.LENGTH_SHORT).show()
                    }
                }
            )
        }
    }

    suspend fun onModuleUndoUninstall(module: ModuleViewModel.ModuleInfo) {

        val success = loadingDialog.withLoading {
            withContext(Dispatchers.IO) {
                undoUninstallModule(module.id)
            }
        }

        if (success) {
            viewModel.fetchModuleList()
        }
        val message = if (success) {
            successUndoUninstall.format(module.name)
        } else {
            failedUndoUninstall.format(module.name)
        }
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
    }

    suspend fun onModuleUninstall(module: ModuleViewModel.ModuleInfo) {
        val formatter = if (module.metamodule) metaModuleUninstallConfirm else moduleUninstallConfirm
        val confirmResult = confirmDialog.awaitConfirm(
            moduleStr,
            content = formatter.format(module.name),
            confirm = uninstall,
            dismiss = cancel
        )
        if (confirmResult != ConfirmResult.Confirmed) {
            return
        }

        val success = loadingDialog.withLoading {
            withContext(Dispatchers.IO) {
                uninstallModule(module.id)
            }
        }

        if (success) {
            viewModel.fetchModuleList()
        }
        val message = if (success) {
            successUninstall.format(module.name)
        } else {
            failedUninstall.format(module.name)
        }
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
    }

    suspend fun onModuleToggle(module: ModuleViewModel.ModuleInfo) {
        val success = loadingDialog.withLoading {
            withContext(Dispatchers.IO) {
                toggleModule(module.id, !module.enabled)
            }
        }
        if (success) {
            viewModel.fetchModuleList()
            Toast.makeText(context, rebootToApply, Toast.LENGTH_SHORT).show()
        } else {
            val message = if (module.enabled) failedDisable else failedEnable
            Toast.makeText(context, message.format(module.name), Toast.LENGTH_SHORT).show()
        }
    }

    fun onModuleClick(id: String, name: String, hasWebUi: Boolean) {
        if (hasWebUi) {
            webUILauncher.launch(
                Intent(context, WebUIActivity::class.java)
                    .setData("kernelsu://webui/$id".toUri())
                    .putExtra("id", id)
                    .putExtra("name", name)
            )
        }
    }

    fun onModuleAddShortcut(module: ModuleViewModel.ModuleInfo) {
        shortcutModuleId = module.id
        shortcutName = module.name
        shortcutIconUri = null
        defaultShortcutIconUri = null
        defaultActionShortcutIconUri = module.actionIconPath
            ?.takeIf { it.isNotBlank() }
            ?.let { "su:$it" }
        defaultWebUiShortcutIconUri = module.webUiIconPath
            ?.takeIf { it.isNotBlank() }
            ?.let { "su:$it" }
        if (module.hasActionScript && module.hasWebUi) {
            selectedShortcutType = null
            showShortcutTypeDialog.value = true
        } else if (module.hasActionScript) {
            openShortcutDialogForType(ShortcutType.Action)
        } else if (module.hasWebUi) {
            openShortcutDialogForType(ShortcutType.WebUI)
        }
    }

    val listState = rememberLazyListState()
    val nestedScrollConnection = remember {
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

    val hazeState = remember { HazeState() }
    val hazeStyle = if (enableBlur) {
        HazeStyle(
            backgroundColor = colorScheme.surface,
            tint = HazeTint(colorScheme.surface.copy(0.8f))
        )
    } else {
        HazeStyle.Unspecified
    }

    Scaffold(
        topBar = {
            searchStatus.TopAppBarAnim(hazeState = hazeState, hazeStyle = hazeStyle) {
                TopAppBar(
                    color = if (enableBlur) Color.Transparent else colorScheme.surface,
                    title = stringResource(R.string.module),
                    actions = {
                        Box {
                            val showTopPopup = remember { mutableStateOf(false) }
                            IconButton(
                                modifier = Modifier.padding(end = 8.dp),
                                onClick = { showTopPopup.value = true },
                                holdDownState = showTopPopup.value
                            ) {
                                Icon(
                                    imageVector = MiuixIcons.MoreCircle,
                                    tint = colorScheme.onSurface,
                                    contentDescription = null
                                )
                            }
                            SuperListPopup(
                                show = showTopPopup,
                                popupPositionProvider = MenuPositionProvider,
                                alignment = PopupPositionProvider.Align.TopEnd,
                                onDismissRequest = {
                                    showTopPopup.value = false
                                }
                            ) {
                                ListPopupColumn {
                                    DropdownImpl(
                                        text = stringResource(R.string.module_sort_action_first),
                                        optionSize = 2,
                                        isSelected = uiState.sortActionFirst,
                                        onSelectedIndexChange = {
                                            val newValue = !uiState.sortActionFirst
                                            viewModel.setSortActionFirst(newValue)
                                            prefs.edit {
                                                putBoolean("module_sort_action_first", newValue)
                                            }
                                            scope.launch {
                                                viewModel.fetchModuleList()
                                            }
                                            showTopPopup.value = false
                                        },
                                        index = 0
                                    )
                                    DropdownImpl(
                                        text = stringResource(R.string.module_sort_enabled_first),
                                        optionSize = 2,
                                        isSelected = uiState.sortEnabledFirst,
                                        onSelectedIndexChange = {
                                            val newValue = !uiState.sortEnabledFirst
                                            viewModel.setSortEnabledFirst(newValue)
                                            prefs.edit {
                                                putBoolean("module_sort_enabled_first", newValue)
                                            }
                                            scope.launch {
                                                viewModel.fetchModuleList()
                                            }
                                            showTopPopup.value = false
                                        },
                                        index = 1
                                    )
                                }
                            }
                        }
                        RebootListPopup(
                            modifier = Modifier.padding(end = 16.dp),
                            alignment = PopupPositionProvider.Align.TopEnd,
                        )
                    },
                    navigationIcon = {
                        IconButton(
                            modifier = Modifier.padding(start = 16.dp),
                            onClick = { navigator.push(Route.ModuleRepo) },
                        ) {
                            Icon(
                                imageVector = MiuixIcons.Download,
                                tint = colorScheme.onSurface,
                                contentDescription = null
                            )
                        }
                    },
                    scrollBehavior = scrollBehavior
                )
            }
        },
        floatingActionButton = {
            if (!hideInstallButton) {
                val moduleInstall = stringResource(id = R.string.module_install)
                val confirmTitle = stringResource(R.string.module)
                var zipUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
                val confirmDialog = rememberConfirmDialog(
                    onConfirm = {
                        navigator.push(Route.Flash(FlashIt.FlashModules(zipUris)))
                        viewModel.markNeedRefresh()
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
                        navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uris.first()))))
                        viewModel.markNeedRefresh()
                    } else if (uris.size > 1) {
                        // multiple files selected
                        zipUris = uris
                        val moduleNames = uris.mapIndexed { index, uri -> "\n${index + 1}. ${uri.getFileName(context)}" }.joinToString("")
                        val confirmContent = context.getString(R.string.module_install_prompt_with_name, moduleNames)
                        confirmDialog.showConfirm(
                            title = confirmTitle,
                            content = confirmContent
                        )
                    }
                }
                FloatingActionButton(
                    modifier = Modifier
                        .offset(y = offsetHeight)
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
                onSearchStatusChange = viewModel::updateSearchStatus,
                defaultResult = {},
                searchBarTopPadding = dynamicTopPadding,
            ) {
                item {
                    Spacer(Modifier.height(6.dp))
                }
                items(
                    uiState.searchResults,
                    key = { it.id },
                    contentType = { "module" }
                ) { module ->
                    AnimatedVisibility(
                        visible = uiState.searchResults.isNotEmpty(),
                        enter = fadeIn() + expandVertically(),
                        exit = fadeOut() + shrinkVertically()
                    ) {
                        val itemScope = rememberCoroutineScope()
                        val updateInfoMap = uiState.updateInfo
                        val currentModuleState = rememberUpdatedState(module)
                        val moduleUpdateInfo = updateInfoMap[module.id] ?: ModuleViewModel.ModuleUpdateInfo.Empty

                        val onUninstallClick = remember(module.id, itemScope, ::onModuleUninstall) {
                            {
                                itemScope.launch {
                                    onModuleUninstall(currentModuleState.value)
                                }
                                Unit
                            }
                        }
                        val onUndoUninstallClick = remember(module.id, itemScope, ::onModuleUndoUninstall) {
                            {
                                itemScope.launch {
                                    onModuleUndoUninstall(currentModuleState.value)
                                }
                                Unit
                            }
                        }
                        val onToggleClick = remember(module.id, itemScope, ::onModuleToggle) {
                            { _: Boolean ->
                                itemScope.launch {
                                    onModuleToggle(currentModuleState.value)
                                }
                                Unit
                            }
                        }
                        val onUpdateClick = remember(module.id, moduleUpdateInfo, itemScope, ::onModuleUpdate, context, navigator) {
                            {
                                itemScope.launch {
                                    onModuleUpdate(
                                        currentModuleState.value,
                                        moduleUpdateInfo.changelog,
                                        moduleUpdateInfo.downloadUrl,
                                        "${currentModuleState.value.name}-${moduleUpdateInfo.version}.zip",
                                        context
                                    ) { uri ->
                                        navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri))))
                                        viewModel.markNeedRefresh()
                                    }
                                }
                                Unit
                            }
                        }
                        val onExecuteActionClick = remember(module.id, navigator, viewModel) {
                            {
                                navigator.push(Route.ExecuteModuleAction(currentModuleState.value.id))
                                viewModel.markNeedRefresh()
                            }
                        }
                        val onAddShortcutClick = remember(module.id) {
                            {
                                onModuleAddShortcut(currentModuleState.value)
                            }
                        }
                        val onOpenWebUiClick = remember(module.id) {
                            {
                                onModuleClick(
                                    currentModuleState.value.id,
                                    currentModuleState.value.name,
                                    currentModuleState.value.hasWebUi
                                )
                            }
                        }
                        ModuleItem(
                            module = module,
                            updateUrl = moduleUpdateInfo.downloadUrl,
                            onUndoUninstall = onUndoUninstallClick,
                            onUninstall = onUninstallClick,
                            onCheckChanged = onToggleClick,
                            onUpdate = onUpdateClick,
                            onExecuteAction = onExecuteActionClick,
                            onAddActionShortcut = onAddShortcutClick,
                            onOpenWebUi = onOpenWebUiClick
                        )
                    }
                }
                item {
                    val imeBottomPadding = WindowInsets.ime.asPaddingValues().calculateBottomPadding()
                    Spacer(Modifier.height(maxOf(bottomInnerPadding, imeBottomPadding)))
                }
            }
        },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        when {
            magiskInstalled -> {
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
            }

            else -> {
                val layoutDirection = LocalLayoutDirection.current
                searchStatus.SearchBox(
                    onSearchStatusChange = viewModel::updateSearchStatus,
                    searchBarTopPadding = dynamicTopPadding,
                    contentPadding = PaddingValues(
                        top = innerPadding.calculateTopPadding(),
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection)
                    ),
                    hazeState = hazeState,
                    hazeStyle = hazeStyle
                ) { boxHeight ->
                    ModuleList(
                        navigator = navigator,
                        viewModel = viewModel,
                        modifier = Modifier
                            .fillMaxHeight()
                            .scrollEndHaptic()
                            .overScrollVertical()
                            .nestedScroll(scrollBehavior.nestedScrollConnection)
                            .nestedScroll(nestedScrollConnection)
                            .let { if (enableBlur) it.hazeSource(state = hazeState) else it },
                        scope = scope,
                        modules = modules,
                        onClickModule = { id, name, hasWebUi ->
                            onModuleClick(id, name, hasWebUi)
                        },
                        onModuleUninstall = { module ->
                            onModuleUninstall(module)
                        },
                        onModuleUndoUninstall = { module ->
                            onModuleUndoUninstall(module)
                        },
                        onModuleToggle = { module ->
                            onModuleToggle(module)
                        },
                        onModuleUpdate = { module, changelogUrl, downloadUrl, fileName ->
                            onModuleUpdate(
                                module,
                                changelogUrl,
                                downloadUrl,
                                fileName,
                                context
                            ) { uri ->
                                navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri))))
                                viewModel.markNeedRefresh()
                            }
                        },
                        onModuleAddShortcut = { module ->
                            onModuleAddShortcut(module)
                        },
                        context = context,
                        innerPadding = innerPadding,
                        bottomInnerPadding = bottomInnerPadding,
                        boxHeight = boxHeight
                    )
                }
            }
        }
    }
    if (showShortcutTypeDialog.value) {
        SuperDialog(
            show = showShortcutTypeDialog,
            title = stringResource(R.string.module_shortcut_type_title),
            onDismissRequest = {
                showShortcutTypeDialog.value = false
            }
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                TextButton(
                    text = "Action",
                    onClick = {
                        showShortcutTypeDialog.value = false
                        openShortcutDialogForType(ShortcutType.Action)
                    },
                    modifier = Modifier.fillMaxWidth()
                )
                TextButton(
                    text = "WebUI",
                    onClick = {
                        showShortcutTypeDialog.value = false
                        openShortcutDialogForType(ShortcutType.WebUI)
                    },
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
    if (showShortcutDialog.value) {
        SuperDialog(
            show = showShortcutDialog,
            title = stringResource(R.string.module_shortcut_title),
            onDismissRequest = {
                showShortcutDialog.value = false
            }
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .padding(vertical = 16.dp)
                        .size(100.dp)
                        .clip(ContinuousRoundedRectangle(25.dp))
                ) {
                    val preview = shortcutPreviewIcon.value
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
                        onClick = { pickShortcutIconLauncher.launch("image/*") },
                    )
                    AnimatedVisibility(
                        visible = shortcutIconUri != defaultShortcutIconUri,
                        enter = expandHorizontally() + slideInHorizontally(initialOffsetX = { it }),
                        exit = shrinkHorizontally() + slideOutHorizontally(targetOffsetX = { it }),
                        modifier = Modifier.align(Alignment.CenterVertically),
                    ) {
                        IconButton(
                            onClick = { shortcutIconUri = defaultShortcutIconUri },
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
                    value = shortcutName,
                    onValueChange = { shortcutName = it },
                    label = stringResource(id = R.string.module_shortcut_name_label)
                )
                if (hasExistingShortcut) {
                    TextButton(
                        text = stringResource(id = R.string.module_shortcut_delete),
                        onClick = {
                            val moduleId = shortcutModuleId
                            val type = selectedShortcutType
                            if (!moduleId.isNullOrBlank() && type != null) {
                                deleteModuleShortcut(context, moduleId, type)
                            }
                            showShortcutDialog.value = false
                        },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                Row(
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    TextButton(
                        text = stringResource(id = android.R.string.cancel),
                        onClick = { showShortcutDialog.value = false },
                        modifier = Modifier.weight(1f),
                    )
                    TextButton(
                        text = if (hasExistingShortcut) {
                            stringResource(id = R.string.module_update)
                        } else {
                            stringResource(id = android.R.string.ok)
                        },
                        onClick = {
                            val moduleId = shortcutModuleId
                            val type = selectedShortcutType
                            if (!moduleId.isNullOrBlank() && shortcutName.isNotBlank() && type != null) {
                                createModuleShortcut(
                                    context = context,
                                    moduleId = moduleId,
                                    name = shortcutName,
                                    iconUri = shortcutIconUri,
                                    type = type
                                )
                            }
                            showShortcutDialog.value = false
                        },
                        colors = ButtonDefaults.textButtonColorsPrimary(),
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        }
    }
}

@Composable
private fun ModuleList(
    navigator: Navigator,
    viewModel: ModuleViewModel,
    modifier: Modifier = Modifier,
    scope: CoroutineScope,
    modules: List<ModuleViewModel.ModuleInfo>,
    onClickModule: (id: String, name: String, hasWebUi: Boolean) -> Unit,
    onModuleUninstall: suspend (ModuleViewModel.ModuleInfo) -> Unit,
    onModuleUndoUninstall: suspend (ModuleViewModel.ModuleInfo) -> Unit,
    onModuleToggle: suspend (ModuleViewModel.ModuleInfo) -> Unit,
    onModuleUpdate: suspend (ModuleViewModel.ModuleInfo, String, String, String) -> Unit,
    onModuleAddShortcut: (ModuleViewModel.ModuleInfo) -> Unit,
    context: Context,
    innerPadding: PaddingValues,
    bottomInnerPadding: Dp,
    boxHeight: MutableState<Dp>
) {
    val uiState by viewModel.uiState.collectAsState()
    val layoutDirection = LocalLayoutDirection.current
    val updateInfoMap = uiState.updateInfo

    var isRefreshing by rememberSaveable { mutableStateOf(false) }
    val pullToRefreshState = rememberPullToRefreshState()
    val refreshTexts = remember {
        listOf(
            context.getString(R.string.refresh_pulling),
            context.getString(R.string.refresh_release),
            context.getString(R.string.refresh_refresh),
            context.getString(R.string.refresh_complete),
        )
    }
    LaunchedEffect(isRefreshing) {
        if (isRefreshing) {
            delay(150)
            viewModel.fetchModuleList()
            isRefreshing = false
        }
    }

    when {
        modules.isEmpty() -> {
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
        }

        else -> {
            PullToRefresh(
                isRefreshing = isRefreshing,
                pullToRefreshState = pullToRefreshState,
                onRefresh = { if (!isRefreshing) isRefreshing = true },
                refreshTexts = refreshTexts,
                contentPadding = PaddingValues(
                    top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                    start = innerPadding.calculateStartPadding(layoutDirection),
                    end = innerPadding.calculateEndPadding(layoutDirection),
                ),
            ) {
                LazyColumn(
                    modifier = modifier
                        .fillMaxHeight(),
                    contentPadding = PaddingValues(
                        top = innerPadding.calculateTopPadding() + boxHeight.value + 6.dp,
                        start = innerPadding.calculateStartPadding(layoutDirection),
                        end = innerPadding.calculateEndPadding(layoutDirection),
                    ),
                    overscrollEffect = null,
                ) {
                    items(
                        items = modules,
                        key = { it.id },
                        contentType = { "module" }
                    ) { module ->
                        val currentModuleState = rememberUpdatedState(module)
                        val moduleUpdateInfo = updateInfoMap[module.id] ?: ModuleViewModel.ModuleUpdateInfo.Empty

                        val onUndoUninstallClick = remember(module.id, scope, onModuleUndoUninstall) {
                            {
                                scope.launch {
                                    onModuleUndoUninstall(currentModuleState.value)
                                }
                                Unit
                            }
                        }
                        val onUninstallClick = remember(module.id, scope, onModuleUninstall) {
                            {
                                scope.launch {
                                    onModuleUninstall(currentModuleState.value)
                                }
                                Unit
                            }
                        }
                        val onToggleClick = remember(module.id, scope, onModuleToggle) {
                            { _: Boolean ->
                                scope.launch {
                                    onModuleToggle(currentModuleState.value)
                                }
                                Unit
                            }
                        }
                        val onUpdateClick = remember(module.id, moduleUpdateInfo, scope, onModuleUpdate) {
                            {
                                scope.launch {
                                    onModuleUpdate(
                                        currentModuleState.value,
                                        moduleUpdateInfo.changelog,
                                        moduleUpdateInfo.downloadUrl,
                                        "${currentModuleState.value.name}-${moduleUpdateInfo.version}.zip",
                                    )
                                }
                                Unit
                            }
                        }
                        val onExecuteActionClick = remember(module.id, navigator, viewModel) {
                            {
                                navigator.push(Route.ExecuteModuleAction(currentModuleState.value.id))
                                viewModel.markNeedRefresh()
                            }
                        }
                        val onAddShortcutClick = remember(module.id) {
                            {
                                onModuleAddShortcut(currentModuleState.value)
                            }
                        }
                        val onOpenWebUiClick = remember(module.id, onClickModule) {
                            {
                                onClickModule(
                                    currentModuleState.value.id,
                                    currentModuleState.value.name,
                                    currentModuleState.value.hasWebUi
                                )
                            }
                        }

                        ModuleItem(
                            module = module,
                            updateUrl = moduleUpdateInfo.downloadUrl,
                            onUninstall = onUninstallClick,
                            onUndoUninstall = onUndoUninstallClick,
                            onCheckChanged = onToggleClick,
                            onUpdate = onUpdateClick,
                            onExecuteAction = onExecuteActionClick,
                            onAddActionShortcut = onAddShortcutClick,
                            onOpenWebUi = onOpenWebUiClick
                        )
                    }
                    item {
                        Spacer(Modifier.height(bottomInnerPadding))
                    }
                }
            }
        }
    }
}

@Composable
fun ModuleItem(
    module: ModuleViewModel.ModuleInfo,
    updateUrl: String,
    onUndoUninstall: () -> Unit,
    onUninstall: () -> Unit,
    onCheckChanged: (Boolean) -> Unit,
    onUpdate: () -> Unit,
    onExecuteAction: () -> Unit,
    onAddActionShortcut: () -> Unit,
    onOpenWebUi: () -> Unit
) {
    val secondaryContainer = colorScheme.secondaryContainer.copy(alpha = 0.8f)
    val actionIconTint = colorScheme.onSurface.copy(alpha = if (isInDarkTheme()) 0.7f else 0.9f)
    val updateBg = colorScheme.tertiaryContainer.copy(alpha = 0.6f)
    val updateTint = colorScheme.onTertiaryContainer.copy(alpha = 0.8f)
    val hasUpdate by remember(updateUrl) { derivedStateOf { updateUrl.isNotEmpty() } }
    val textDecoration by remember(module.remove) {
        mutableStateOf(if (module.remove) TextDecoration.LineThrough else null)
    }
    val hasDescription by remember(module.description) {
        derivedStateOf { module.description.isNotBlank() }
    }
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
                                    .clip(ContinuousRoundedRectangle(6.dp))
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
                        IconButton(
                            backgroundColor = secondaryContainer,
                            minHeight = 35.dp,
                            minWidth = 35.dp,
                            onClick = onExecuteAction,
                        ) {
                            Icon(
                                modifier = Modifier.size(20.dp),
                                imageVector = Icons.Rounded.PlayArrow,
                                tint = actionIconTint,
                                contentDescription = stringResource(R.string.action)
                            )
                        }
                    }
                    if (module.hasWebUi) {
                        IconButton(
                            backgroundColor = secondaryContainer,
                            minHeight = 35.dp,
                            minWidth = 35.dp,
                            onClick = onOpenWebUi,
                        ) {
                            Icon(
                                modifier = Modifier.size(20.dp),
                                imageVector = Icons.Rounded.Code,
                                tint = actionIconTint,
                                contentDescription = stringResource(R.string.open)
                            )
                        }
                    }
                    if (module.hasActionScript || module.hasWebUi) {
                        IconButton(
                            backgroundColor = secondaryContainer,
                            minHeight = 35.dp,
                            minWidth = 35.dp,
                            onClick = onAddActionShortcut,
                        ) {
                            Icon(
                                modifier = Modifier.size(20.dp),
                                imageVector = Icons.Rounded.Add,
                                tint = actionIconTint,
                                contentDescription = stringResource(R.string.module_shortcut_add)
                            )
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
                    modifier = Modifier.padding(end = 16.dp),
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
                            modifier = Modifier.padding(start = 4.dp, end = 2.dp),
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
