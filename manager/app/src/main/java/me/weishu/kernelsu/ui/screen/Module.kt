package me.weishu.kernelsu.ui.screen

import android.app.Activity.RESULT_OK
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.border
import androidx.compose.foundation.isSystemInDarkTheme
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
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
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
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Code
import androidx.compose.material.icons.rounded.Download
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.edit
import androidx.core.net.toUri
import androidx.lifecycle.viewmodel.compose.viewModel
import com.ramcosta.composedestinations.generated.destinations.ExecuteModuleActionScreenDestination
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.ConfirmResult
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rememberLoadingDialog
import me.weishu.kernelsu.ui.util.DownloadListener
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.util.hasMagisk
import me.weishu.kernelsu.ui.util.toggleModule
import me.weishu.kernelsu.ui.util.uninstallModule
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import me.weishu.kernelsu.ui.webui.WebUIActivity
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.FloatingActionButton
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopup
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Switch
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import top.yukonga.miuix.kmp.extra.DropdownImpl
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.ImmersionMore
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun ModulePager(
    navigator: DestinationsNavigator,
    bottomInnerPadding: Dp
) {
    val viewModel = viewModel<ModuleViewModel>()
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)

    LaunchedEffect(Unit) {
        if (viewModel.moduleList.isEmpty() || viewModel.isNeedRefresh) {
            viewModel.sortEnabledFirst = prefs.getBoolean("module_sort_enabled_first", false)
            viewModel.sortActionFirst = prefs.getBoolean("module_sort_action_first", false)
            viewModel.fetchModuleList()
        }
    }

    val isSafeMode = Natives.isSafeMode
    val hasMagisk = hasMagisk()

    val hideInstallButton = isSafeMode || hasMagisk

    val scrollBehavior = MiuixScrollBehavior()

    val webUILauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { viewModel.fetchModuleList() }

    val listState = rememberLazyListState()
    var fabVisible by remember { mutableStateOf(true) }
    var scrollDistance by remember { mutableFloatStateOf(0f) }
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

    Scaffold(
        topBar = {
            TopAppBar(
                title = stringResource(R.string.module),
                actions = {
                    val showTopPopup = remember { mutableStateOf(false) }
                    ListPopup(
                        show = showTopPopup,
                        popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                        alignment = PopupPositionProvider.Align.TopRight,
                        onDismissRequest = {
                            showTopPopup.value = false
                        }
                    ) {
                        ListPopupColumn {
                            DropdownImpl(
                                text = stringResource(R.string.module_sort_action_first),
                                optionSize = 2,
                                isSelected = viewModel.sortActionFirst,
                                onSelectedIndexChange = {
                                    viewModel.sortActionFirst =
                                        !viewModel.sortActionFirst
                                    prefs.edit {
                                        putBoolean(
                                            "module_sort_action_first",
                                            viewModel.sortActionFirst
                                        )
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
                                isSelected = viewModel.sortEnabledFirst,
                                onSelectedIndexChange = {
                                    viewModel.sortEnabledFirst =
                                        !viewModel.sortEnabledFirst
                                    prefs.edit {
                                        putBoolean(
                                            "module_sort_enabled_first",
                                            viewModel.sortEnabledFirst
                                        )
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
                    IconButton(
                        modifier = Modifier.padding(end = 16.dp),
                        onClick = { showTopPopup.value = true },
                        holdDownState = showTopPopup.value
                    ) {
                        Icon(
                            imageVector = MiuixIcons.Useful.ImmersionMore,
                            tint = colorScheme.onSurface,
                            contentDescription = stringResource(id = R.string.settings)
                        )

                    }
                },
                scrollBehavior = scrollBehavior
            )
        },
        floatingActionButton = {
            if (!hideInstallButton) {
                val moduleInstall = stringResource(id = R.string.module_install)
                val confirmTitle = stringResource(R.string.module)
                var zipUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
                val confirmDialog = rememberConfirmDialog(onConfirm = {
                    navigator.navigate(FlashScreenDestination(FlashIt.FlashModules(zipUris))) {
                        launchSingleTop = true
                    }
                    viewModel.markNeedRefresh()
                })
                val selectZipLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.StartActivityForResult()
                ) {
                    if (it.resultCode != RESULT_OK) {
                        return@rememberLauncherForActivityResult
                    }
                    val data = it.data ?: return@rememberLauncherForActivityResult
                    val clipData = data.clipData

                    val uris = mutableListOf<Uri>()
                    if (clipData != null) {
                        for (i in 0 until clipData.itemCount) {
                            clipData.getItemAt(i)?.uri?.let { it -> uris.add(it) }
                        }
                    } else {
                        data.data?.let { it -> uris.add(it) }
                    }

                    if (uris.size == 1) {
                        navigator.navigate(FlashScreenDestination(FlashIt.FlashModules(listOf(uris.first())))) {
                            launchSingleTop = true
                        }
                    } else if (uris.size > 1) {
                        // multiple files selected
                        val moduleNames =
                            uris.mapIndexed { index, uri -> "\n${index + 1}. ${uri.getFileName(context)}" }.joinToString("")
                        val confirmContent = context.getString(R.string.module_install_prompt_with_name, moduleNames)
                        zipUris = uris
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
                            tint = Color.White
                        )
                    },
                )
            }
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        when {
            hasMagisk -> {
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
                ModuleList(
                    navigator,
                    viewModel = viewModel,
                    modifier = Modifier
                        .height(getWindowSize().height.dp)
                        .scrollEndHaptic()
                        .overScrollVertical()
                        .nestedScroll(nestedScrollConnection)
                        .nestedScroll(scrollBehavior.nestedScrollConnection)
                        .padding(horizontal = 12.dp),
                    onInstallModule = {
                        navigator.navigate(FlashScreenDestination(FlashIt.FlashModules(listOf(it)))) {
                            launchSingleTop = true
                        }
                    },
                    onClickModule = { id, name, hasWebUi ->
                        if (hasWebUi) {
                            webUILauncher.launch(
                                Intent(context, WebUIActivity::class.java)
                                    .setData("kernelsu://webui/$id".toUri())
                                    .putExtra("id", id)
                                    .putExtra("name", name)
                            )
                        }
                    },
                    context = context,
                    innerPadding = innerPadding,
                    bottomInnerPadding = bottomInnerPadding
                )
            }
        }
    }
}

@Composable
private fun ModuleList(
    navigator: DestinationsNavigator,
    viewModel: ModuleViewModel,
    modifier: Modifier = Modifier,
    onInstallModule: (Uri) -> Unit,
    onClickModule: (id: String, name: String, hasWebUi: Boolean) -> Unit,
    context: Context,
    innerPadding: PaddingValues,
    bottomInnerPadding: Dp
) {
    val failedEnable = stringResource(R.string.module_failed_to_enable)
    val failedDisable = stringResource(R.string.module_failed_to_disable)
    val failedUninstall = stringResource(R.string.module_uninstall_failed)
    val successUninstall = stringResource(R.string.module_uninstall_success)
    stringResource(R.string.reboot)
    val rebootToApply = stringResource(R.string.reboot_to_apply)
    val moduleStr = stringResource(R.string.module)
    val uninstall = stringResource(R.string.uninstall)
    val cancel = stringResource(android.R.string.cancel)
    val moduleUninstallConfirm = stringResource(R.string.module_uninstall_confirm)
    val updateText = stringResource(R.string.module_update)
    val changelogText = stringResource(R.string.module_changelog)
    val downloadingText = stringResource(R.string.module_downloading)
    val startDownloadingText = stringResource(R.string.module_start_downloading)
    val fetchChangeLogFailed = stringResource(R.string.module_changelog_failed)

    val loadingDialog = rememberLoadingDialog()
    val confirmDialog = rememberConfirmDialog()

    suspend fun onModuleUpdate(
        module: ModuleViewModel.ModuleInfo,
        changelogUrl: String,
        downloadUrl: String,
        fileName: String
    ) {
        val changelogResult = loadingDialog.withLoading {
            withContext(Dispatchers.IO) {
                runCatching {
                    ksuApp.okhttpClient.newCall(
                        okhttp3.Request.Builder().url(changelogUrl).build()
                    ).execute().body!!.string()
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

        val changelog = changelogResult.getOrElse {
            showToast(fetchChangeLogFailed.format(it.message))
            return
        }.ifBlank {
            showToast(fetchChangeLogFailed.format(module.name))
            return
        }

        // changelog is not empty, show it and wait for confirm
        val confirmResult = confirmDialog.awaitConfirm(
            changelogText,
            content = changelog,
            markdown = true,
            confirm = updateText,
        )

        if (confirmResult != ConfirmResult.Confirmed) {
            return
        }

        showToast(startDownloadingText.format(module.name))

        val downloading = downloadingText.format(module.name)
        withContext(Dispatchers.IO) {
            download(
                context,
                downloadUrl,
                fileName,
                downloading,
                onDownloaded = onInstallModule,
                onDownloading = {
                    launch(Dispatchers.Main) {
                        Toast.makeText(context, downloading, Toast.LENGTH_SHORT).show()
                    }
                }
            )
        }
    }

    suspend fun onModuleUninstall(module: ModuleViewModel.ModuleInfo) {
        val confirmResult = confirmDialog.awaitConfirm(
            moduleStr,
            content = moduleUninstallConfirm.format(module.name),
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

    var isRefreshing by rememberSaveable { mutableStateOf(false) }
    val pullToRefreshState = rememberPullToRefreshState()
    LaunchedEffect(isRefreshing) {
        if (isRefreshing) {
            delay(350)
            viewModel.fetchModuleList()
            isRefreshing = false
        }
    }
    val refreshTexts = listOf(
        stringResource(R.string.refresh_pulling),
        stringResource(R.string.refresh_release),
        stringResource(R.string.refresh_refresh),
        stringResource(R.string.refresh_complete),
    )
    PullToRefresh(
        isRefreshing = isRefreshing,
        pullToRefreshState = pullToRefreshState,
        onRefresh = { isRefreshing = true },
        refreshTexts = refreshTexts,
        contentPadding = PaddingValues(top = innerPadding.calculateTopPadding() + 12.dp)
    ) {
        val layoutDirection = LocalLayoutDirection.current

        LazyColumn(
            modifier = modifier.height(getWindowSize().height.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding() + 12.dp,
                bottom = bottomInnerPadding + 12.dp,
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection)
            ),
            overscrollEffect = null,
        ) {
            when {
                !viewModel.isOverlayAvailable -> {
                    item {
                        Box(
                            modifier = Modifier.fillParentMaxSize(),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                stringResource(R.string.module_overlay_fs_not_available),
                                textAlign = TextAlign.Center
                            )
                        }
                    }
                }

                viewModel.moduleList.isEmpty() -> {
                    item {
                        Box(
                            modifier = Modifier.fillParentMaxSize(),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                stringResource(R.string.module_empty),
                                textAlign = TextAlign.Center
                            )
                        }
                    }
                }

                else -> {
                    items(viewModel.moduleList) { module ->
                        val scope = rememberCoroutineScope()
                        val updatedModule by produceState(initialValue = Triple("", "", "")) {
                            scope.launch(Dispatchers.IO) {
                                value = viewModel.checkUpdate(module)
                            }
                        }

                        ModuleItem(
                            navigator = navigator,
                            module = module,
                            updateUrl = updatedModule.first,
                            onUninstall = {
                                scope.launch { onModuleUninstall(module) }
                            },
                            onCheckChanged = {
                                scope.launch {
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
                            },
                            onUpdate = {
                                scope.launch {
                                    onModuleUpdate(
                                        module,
                                        updatedModule.third,
                                        updatedModule.first,
                                        "${module.name}-${updatedModule.second}.zip"
                                    )
                                }
                            },
                            onClick = {
                                onClickModule(it.id, it.name, it.hasWebUi)
                            }
                        )
                    }
                }
            }
        }

        DownloadListener(context, onInstallModule)

    }
}

@Composable
fun ModuleItem(
    navigator: DestinationsNavigator,
    module: ModuleViewModel.ModuleInfo,
    updateUrl: String,
    onUninstall: (ModuleViewModel.ModuleInfo) -> Unit,
    onCheckChanged: (Boolean) -> Unit,
    onUpdate: (ModuleViewModel.ModuleInfo) -> Unit,
    onClick: (ModuleViewModel.ModuleInfo) -> Unit
) {
    val textDecoration = if (!module.remove) null else TextDecoration.LineThrough
    val viewModel = viewModel<ModuleViewModel>()

    Card(
        modifier = Modifier.fillMaxWidth(),
        insideMargin = PaddingValues(16.dp)
    ) {
        Row(
            modifier = Modifier,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(end = 4.dp)
            ) {
                val moduleVersion = stringResource(id = R.string.module_version)
                val moduleAuthor = stringResource(id = R.string.module_author)

                Text(
                    text = module.name,
                    fontSize = 17.sp,
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurface,
                    textDecoration = textDecoration,
                )

                Text(
                    text = "$moduleVersion: ${module.version}",
                    fontSize = 14.sp,
                    modifier = Modifier.padding(top = 1.dp),
                    fontWeight = FontWeight.Medium,
                    color = colorScheme.onSurfaceVariantSummary,
                    textDecoration = textDecoration,
                )

                Text(
                    text = "$moduleAuthor: ${module.author}",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = colorScheme.onSurfaceVariantSummary,
                    textDecoration = textDecoration,
                )
            }
            Switch(
                modifier = Modifier,
                checked = module.enabled,
                onCheckedChange = onCheckChanged
            )
        }

        Text(
            text = module.description,
            fontSize = 14.5.sp,
            color = colorScheme.onSurfaceVariantSummary,
            modifier = Modifier.padding(top = 2.dp),
            overflow = TextOverflow.Ellipsis,
            maxLines = 4,
            textDecoration = textDecoration
        )

        HorizontalDivider(
            modifier = Modifier.padding(top = 10.dp, bottom = 10.dp),
            thickness = 0.5.dp,
            color = colorScheme.outline.copy(alpha = 0.5f)
        )

        Row(
            modifier = Modifier,
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            AnimatedVisibility(
                visible = module.enabled && !module.remove,
                enter = fadeIn(),
                exit = fadeOut(),
            ) {
                if (module.hasActionScript) {
                    IconButton(
                        backgroundColor = colorScheme.secondaryContainer.copy(alpha = 0.8f),
                        minHeight = 35.dp,
                        minWidth = 35.dp,
                        onClick = {
                            navigator.navigate(ExecuteModuleActionScreenDestination(module.id)) {
                                launchSingleTop = true
                            }
                            viewModel.markNeedRefresh()
                        },
                    ) {
                        Icon(
                            modifier = Modifier.size(20.dp),
                            imageVector = Icons.Rounded.PlayArrow,
                            tint = colorScheme.onSurface.copy(alpha = if (isSystemInDarkTheme()) 0.7f else 0.9f),
                            contentDescription = stringResource(R.string.action)
                        )
                    }
                }

                if (module.hasWebUi) {
                    IconButton(
                        backgroundColor = colorScheme.secondaryContainer.copy(alpha = 0.8f),
                        minHeight = 35.dp,
                        minWidth = 35.dp,
                        onClick = { onClick(module) },
                    ) {
                        Icon(
                            modifier = Modifier.size(20.dp),
                            imageVector = Icons.Rounded.Code,
                            tint = colorScheme.onSurface.copy(alpha = if (isSystemInDarkTheme()) 0.7f else 0.9f),
                            contentDescription = stringResource(R.string.open)
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.weight(1f, true))

            if (updateUrl.isNotEmpty()) {
                IconButton(
                    backgroundColor = Color(if (isSystemInDarkTheme()) 0xFF25354E else 0xFFEAF2FF),
                    enabled = !module.remove,
                    minHeight = 35.dp,
                    minWidth = 35.dp,
                    onClick = { onUpdate(module) },
                ) {
                    Row(
                        modifier = Modifier.then(
                            if (updateUrl.isNotEmpty()) {
                                Modifier.padding(horizontal = 10.dp)
                            } else {
                                Modifier
                            }
                        ),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(2.dp),
                    ) {
                        Icon(
                            modifier = Modifier.size(20.dp),
                            imageVector = Icons.Rounded.Download,
                            tint = Color(0xFF0D84FF),
                            contentDescription = stringResource(R.string.module_update),
                        )
                        Text(
                            text = stringResource(R.string.module_update),
                            color = Color(0xFF0D84FF),
                            fontWeight = FontWeight.Medium,
                            fontSize = 14.sp
                        )
                    }
                }
            }

            IconButton(
                enabled = !module.remove,
                minHeight = 35.dp,
                minWidth = 35.dp,
                onClick = { onUninstall(module) },
                backgroundColor = colorScheme.secondaryContainer.copy(alpha = 0.8f),
            ) {
                Row(
                    modifier = Modifier.then(
                        if (updateUrl.isEmpty()) {
                            Modifier.padding(horizontal = 10.dp)
                        } else {
                            Modifier
                        }
                    ),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                ) {
                    Icon(
                        modifier = Modifier
                            .size(20.dp),
                        imageVector = Icons.Outlined.Delete,
                        tint = colorScheme.onSurface.copy(alpha = if (isSystemInDarkTheme()) 0.7f else 0.9f),
                        contentDescription = null
                    )
                    if (updateUrl.isEmpty()) {
                        Text(
                            modifier = Modifier.padding(end = 3.dp),
                            text = stringResource(R.string.uninstall),
                            color = colorScheme.onSurface.copy(alpha = if (isSystemInDarkTheme()) 0.7f else 0.9f),
                            fontWeight = FontWeight.Medium,
                            fontSize = 15.sp
                        )
                    }
                }
            }
        }
    }
}
