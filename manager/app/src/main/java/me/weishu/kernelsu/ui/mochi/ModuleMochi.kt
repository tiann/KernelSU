package me.weishu.kernelsu.ui.mochi

import android.app.Activity.RESULT_OK
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.data.model.ModuleUpdateInfo
import me.weishu.kernelsu.data.repository.isSoftRebootPreferred
import me.weishu.kernelsu.ui.component.ObserveAsEvents
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.screen.module.ModuleActions
import me.weishu.kernelsu.ui.screen.module.ModuleConfirmDialogState
import me.weishu.kernelsu.ui.screen.module.ModuleConfirmRequest
import me.weishu.kernelsu.ui.screen.module.ModuleEffect
import me.weishu.kernelsu.ui.screen.module.ModuleShortcutState
import me.weishu.kernelsu.ui.screen.module.ModuleUiState
import me.weishu.kernelsu.ui.screen.module.preinitWarning
import me.weishu.kernelsu.ui.screen.module.rememberModuleShortcutState
import me.weishu.kernelsu.ui.theme.LocalModuleDescriptionMaxLines
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.screen.module.ShortcutType
import me.weishu.kernelsu.ui.util.reboot
import me.weishu.kernelsu.ui.util.DownloadManager
import ui.feice.mochi.basic.MochiBarIconButton
import ui.feice.mochi.basic.MochiBarGroupDropdownItem
import ui.feice.mochi.basic.MochiBarGroupItem
import ui.feice.mochi.basic.MochiBarGroup
import ui.feice.mochi.basic.MochiBarIconDropdown
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonDefaults
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.MochiDownloadButton
import ui.feice.mochi.basic.MochiDownloadState
import ui.feice.mochi.basic.MochiContextMenu
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiEmptyState
import ui.feice.mochi.basic.MochiItem
import ui.feice.mochi.basic.MochiSearchBar
import ui.feice.mochi.basic.MochiSwitch
import ui.feice.mochi.basic.MochiTextField
import ui.feice.mochi.basic.MochiToastHost
import ui.feice.mochi.basic.Text
import ui.feice.mochi.basic.rememberMochiToastState
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListDefaults
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold
import ui.feice.mochi.shape.MochiRoundedRectangle
import ui.feice.mochi.theme.MochiTheme

@Composable
fun ModulePagerMochi(
    uiState: ModuleUiState,
    confirmDialogState: ModuleConfirmDialogState?,
    moduleEvent: Flow<ModuleEffect>,
    actions: ModuleActions,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val downloads by DownloadManager.downloads.collectAsStateWithLifecycle()
    val toast = rememberMochiToastState()
    val loading = rememberLoadingDialog()
    val refreshing = rememberUpdatedState(uiState.isRefreshing)
    val query = uiState.searchStatus.searchText
    val searching = query.isNotBlank()
    val modules = if (searching) uiState.searchResults else uiState.moduleList

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
    LaunchedEffect(confirmDialogState) {
        confirmDialogState?.let {
            confirmDialog.showConfirm(
                title = it.title,
                content = it.content,
                markdown = it.markdown,
                html = it.html,
                confirm = it.confirm,
                dismiss = it.dismiss,
                restricted = it.restricted,
            )
        }
    }

    // Messages that need a reboot reboot on tap
    val rebootLabel = stringResource(R.string.reboot)
    val softRebootLabel = stringResource(R.string.reboot_soft)
    ObserveAsEvents(moduleEvent) { event ->
        when (event) {
            is ModuleEffect.Toast -> toast.show(event.message)
            is ModuleEffect.SnackBar -> {
                val soft = isSoftRebootPreferred()
                toast.show(
                    event.message,
                    subtitle = if (soft) softRebootLabel else rebootLabel,
                    icon = { tint -> MochiIonicon("power", tint = tint) },
                    durationMillis = 6000,
                    onClick = { reboot(if (soft) "soft_reboot" else "") },
                )
            }
        }
    }

    val installTitle = stringResource(R.string.module)
    val installPrompt = stringResource(R.string.module_install_prompt_with_name, "%s")
    var zipUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    val installConfirm = rememberConfirmDialog(onConfirm = { actions.onOpenFlash(zipUris) })
    val selectZip = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode != RESULT_OK) return@rememberLauncherForActivityResult
        val data = result.data ?: return@rememberLauncherForActivityResult
        val uris = buildList {
            val clip = data.clipData
            if (clip != null) {
                for (i in 0 until clip.itemCount) clip.getItemAt(i)?.uri?.let(::add)
            } else {
                data.data?.let(::add)
            }
        }
        when {
            uris.size == 1 -> actions.onOpenFlash(uris)
            uris.size > 1 -> {
                zipUris = uris
                val names = uris.mapIndexed { i, uri -> "\n${i + 1}. ${uri.getFileName(context)}" }.joinToString("")
                installConfirm.showConfirm(title = installTitle, content = installPrompt.format(names))
            }
        }
    }

    val shortcutState = rememberModuleShortcutState(context)
    var showShortcut by remember { mutableStateOf(false) }
    val pickIcon = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        shortcutState.updateIconUri(uri?.toString())
    }
    val addShortcut: (Module, ShortcutType) -> Unit = { module, type ->
        shortcutState.bindModule(module)
        shortcutState.selectType(type)
        showShortcut = true
    }

    var menu by remember { mutableStateOf(false) }
    val repoLabel = stringResource(R.string.module_repos)
    val installLabel = stringResource(R.string.module_install)
    val moreLabel = stringResource(R.string.mochi_more)

    Box(Modifier.fillMaxSize()) {
        MochiLazyLargeTitleScaffold(
            title = stringResource(R.string.module),
            actions = {
              MochiBarGroup(navId = "module.actions") {
                MochiBarGroupItem(
                    onClick = actions.onOpenRepo,
                    modifier = Modifier.semantics { contentDescription = repoLabel },
                    navId = "module.repo",
                    contentKey = "storefront",
                ) { MochiIonicon("storefront") }
                if (uiState.installButtonVisible) {
                    MochiBarGroupItem(
                        onClick = {
                            selectZip.launch(
                                Intent(Intent.ACTION_GET_CONTENT).apply {
                                    type = "application/zip"
                                    putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                                },
                            )
                        },
                        modifier = Modifier.semantics { contentDescription = installLabel },
                        navId = "module.install",
                        contentKey = "add",
                    ) { MochiIonicon("add") }
                }
                MochiBarGroupDropdownItem(
                    expanded = menu,
                    onExpandedChange = { menu = it },
                    items = persistentListOf(
                        MochiItem(stringResource(R.string.module_sort_action_first), icon = checkIcon(uiState.sortActionFirst)),
                        MochiItem(stringResource(R.string.module_sort_enabled_first), icon = checkIcon(uiState.sortEnabledFirst)),
                    ),
                    onItemClick = { if (it == 0) actions.onToggleSortActionFirst() else actions.onToggleSortEnabledFirst() },
                    modifier = Modifier.semantics { contentDescription = moreLabel },
                    navId = "module.more",
                    contentKey = "filter",
                ) { MochiIonicon("filter") }
              }
            },
            searchBar = {
                MochiSearchBar(
                    query = query,
                    onQueryChange = actions.onSearchTextChange,
                    placeholder = stringResource(R.string.mochi_search_modules),
                    onCancel = actions.onClearSearch,
                )
            },
            onRefresh = { awaitRefresh(actions.onRefresh, refreshing) },
        ) {
            when {
                uiState.magiskInstalled -> item(key = "magisk") {
                    MochiEmptyState(
                        stringResource(R.string.module_magisk_conflict),
                        icon = { MochiIonicon("warning", size = 44.dp, tint = MochiTheme.colorScheme.error) },
                    )
                }

                modules.isEmpty() && (searching || uiState.hasLoaded) -> item(key = "empty") {
                    MochiEmptyState(
                        stringResource(if (searching) R.string.mochi_no_results else R.string.module_empty),
                        icon = { MochiIonicon(if (searching) "search" else "extension-puzzle", size = 44.dp) },
                    )
                }

                else -> modules.forEach { module ->
                    item(key = module.id) {
                        val current by rememberUpdatedState(module)
                        val updateInfo = uiState.updateInfo[module.id] ?: ModuleUpdateInfo.Empty
                        // Update file for this module, same name as ModuleViewModel uses
                        val updateFile = "${module.name}-${updateInfo.version}.zip"
                        val update = downloads.values.lastOrNull { it.fileName == updateFile && it.status != DownloadManager.Status.FAILED }
                        ModuleCard(
                            module = module,
                            hasUpdate = updateInfo.downloadUrl.isNotEmpty(),
                            update = update,
                            onInstallUpdate = { uri -> actions.onOpenFlash(listOf(uri)) },
                            onToggle = { scope.launch { loading.withLoading { actions.onToggleModule(current) } } },
                            onUpdate = {
                                scope.launch { loading.withLoading { actions.onRequestUpdateConfirmation(current, updateInfo) } }
                            },
                            onUninstall = { actions.onRequestUninstallConfirmation(current) },
                            onUndoUninstall = { scope.launch { loading.withLoading { actions.onUndoUninstallModule(current) } } },
                            onAction = { actions.onExecuteModuleAction(current) },
                            onWebUi = { actions.onOpenWebUi(current) },
                            onShortcut = { type -> addShortcut(current, type) },
                        )
                    }
                }
            }
        }
        MochiToastHost(toast)
    }

    ShortcutDialogMochi(
        show = showShortcut,
        state = shortcutState,
        onDismiss = { showShortcut = false },
        onPickIcon = { pickIcon.launch("image/*") },
        onDelete = { shortcutState.deleteShortcut(context) },
        onConfirm = { shortcutState.createShortcut(context) },
    )
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ModuleCard(
    module: Module,
    hasUpdate: Boolean,
    update: DownloadManager.DownloadState?,
    onToggle: () -> Unit,
    onUpdate: () -> Unit,
    onInstallUpdate: (Uri) -> Unit,
    onUninstall: () -> Unit,
    onUndoUninstall: () -> Unit,
    onAction: () -> Unit,
    onWebUi: () -> Unit,
    onShortcut: (ShortcutType) -> Unit,
) {
    val cs = MochiTheme.colorScheme
    val maxLines = LocalModuleDescriptionMaxLines.current
    var expanded by rememberSaveable(module.id) { mutableStateOf(false) }
    val actionLabel = stringResource(R.string.action)
    val openLabel = stringResource(R.string.open)
    val shortcutTitle = stringResource(R.string.module_shortcut_title)

    // Long press creates a shortcut for action / WebUI
    val shortcutTypes = buildList {
        if (module.hasActionScript) add(ShortcutType.Action)
        if (module.hasWebUi) add(ShortcutType.WebUI)
    }
    val menuItems = shortcutTypes.map { type ->
        MochiItem(
            "$shortcutTitle（${if (type == ShortcutType.Action) actionLabel else openLabel}）",
            icon = { tint -> MochiIonicon(if (type == ShortcutType.Action) "play" else "globe", tint = tint) },
        )
    }
    MochiContextMenu(
        items = menuItems,
        onItemClick = { onShortcut(shortcutTypes[it]) },
        enabled = menuItems.isNotEmpty() && module.enabled && !module.remove,
    ) {
        MochiListCard {
            Column(
                Modifier
                    .fillMaxWidth()
                    .padding(start = MochiListDefaults.RowPadding, end = MochiListDefaults.RowPadding, top = 14.dp, bottom = 12.dp),
            ) {
                // Align the switch with the first title line
                Row(verticalAlignment = Alignment.Top) {
                    Text(
                        module.name,
                        style = MochiTheme.textStyles.body.copy(fontWeight = androidx.compose.ui.text.font.FontWeight.SemiBold),
                        color = if (module.remove) cs.onSurfaceVariant else cs.onSurface,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    Spacer(Modifier.width(12.dp))
                    MochiSwitch(
                        module.enabled,
                        { if (it != module.enabled) onToggle() },
                        enabled = !module.update && !module.remove,
                        modifier = Modifier.offset(y = (-3).dp),
                    )
                }
                Row(
                    Modifier.padding(top = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    when {
                        module.remove -> MochiListStatusChip(stringResource(R.string.uninstall), containerColor = cs.error.copy(alpha = 0.12f), contentColor = cs.error)
                        module.metamodule -> MochiListStatusChip("META", containerColor = cs.tertiary.copy(alpha = 0.12f), contentColor = cs.tertiary)
                    }
                    if (module.version.isNotBlank()) {
                        MochiListStatusChip(module.version, containerColor = cs.onSurface.copy(alpha = 0.06f), contentColor = cs.onSurfaceVariant)
                    }
                    if (module.author.isNotBlank()) {
                        Text(
                            module.author,
                            style = MochiTheme.textStyles.footnote,
                            color = cs.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
                module.preinitWarning()?.let { MochiFootnote(stringResource(it), color = cs.error, modifier = Modifier.padding(top = 8.dp)) }
                if (module.description.isNotBlank()) {
                    Text(
                        module.description,
                        modifier = Modifier
                            .padding(top = 10.dp)
                            .clickable(remember { MutableInteractionSource() }, indication = null) { expanded = !expanded },
                        style = MochiTheme.textStyles.subhead.copy(textDecoration = if (module.remove) TextDecoration.LineThrough else null),
                        color = cs.onSurfaceVariant,
                        maxLines = if (expanded) Int.MAX_VALUE else maxLines,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
                Row(
                    Modifier.padding(top = 12.dp).fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    FlowRow(
                        Modifier.weight(1f),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                        itemVerticalAlignment = Alignment.CenterVertically,
                    ) {
                        val usable = module.enabled && !module.remove && !module.update
                        if (usable && module.hasActionScript) CardButton("play", actionLabel, onAction)
                        if (usable && module.hasWebUi) CardButton("globe", openLabel, onWebUi)
                        if (hasUpdate && !module.remove) {
                            val done = update?.resultUri?.takeIf { update.status == DownloadManager.Status.COMPLETED }
                            // update -> progress -> install
                            MochiDownloadButton(
                                state = when {
                                    done != null -> MochiDownloadState.Done
                                    update == null -> MochiDownloadState.Idle
                                    update.status == DownloadManager.Status.PENDING || update.progress <= 0 -> MochiDownloadState.Waiting
                                    else -> MochiDownloadState.Loading(update.progress / 100f)
                                },
                                label = stringResource(R.string.module_update),
                                doneLabel = stringResource(R.string.install),
                                onClick = {
                                    when {
                                        done != null -> onInstallUpdate(done)
                                        update == null -> onUpdate()
                                    }
                                },
                            )
                        }
                        if (module.remove) CardButton("arrow-undo", stringResource(R.string.undo), onUndoUninstall)
                    }
                    if (!module.remove) {
                        val trashLabel = stringResource(R.string.uninstall)
                        MochiButton(
                            onClick = onUninstall,
                            style = MochiButtonStyle.Tonal,
                            colors = MochiButtonDefaults.colors(MochiButtonStyle.Tonal).copy(
                                container = cs.onSurface.copy(alpha = 0.06f),
                                content = cs.error,
                                pressedContainer = cs.error.copy(alpha = 0.16f),
                                glow = cs.error,
                            ),
                            minHeight = CardButtonHeight,
                            contentPadding = PaddingValues(horizontal = 8.dp),
                            modifier = Modifier.semantics { contentDescription = trashLabel },
                        ) {
                            MochiIonicon("trash", size = 16.dp, tint = cs.error)
                        }
                    }
                }
            }
        }
    }
}

private val CardButtonHeight = 32.dp

@Composable
private fun CardButton(icon: String, label: String, onClick: () -> Unit, destructive: Boolean = false) {
    val cs = MochiTheme.colorScheme
    val base = MochiButtonDefaults.colors(MochiButtonStyle.Tonal)
    val colors = if (destructive) {
        base.copy(container = cs.error.copy(alpha = 0.12f), content = cs.error, pressedContainer = cs.error.copy(alpha = 0.2f), glow = cs.error)
    } else {
        base
    }
    MochiButton(
        onClick = onClick,
        style = MochiButtonStyle.Tonal,
        colors = colors,
        minHeight = CardButtonHeight,
        contentPadding = PaddingValues(horizontal = 14.dp),
    ) {
        MochiIonicon(icon, size = 16.dp, tint = colors.content)
        Text(label, style = MochiTheme.textStyles.subhead, color = colors.content)
    }
}

@Composable
private fun ShortcutDialogMochi(
    show: Boolean,
    state: ModuleShortcutState,
    onDismiss: () -> Unit,
    onPickIcon: () -> Unit,
    onDelete: () -> Unit,
    onConfirm: () -> Unit,
) {
    val context = LocalContext.current
    val copied = stringResource(R.string.module_shortcut_scheme_copied)
    MochiDialog(
        visible = show,
        onDismissRequest = onDismiss,
        title = stringResource(R.string.module_shortcut_title),
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel),
            MochiDialogAction(
                stringResource(if (state.hasExistingShortcut) R.string.module_update else android.R.string.ok),
                MochiDialogActionRole.Primary,
                onClick = onConfirm,
            ),
        ),
    ) {
        Column(
            Modifier.fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Box(
                Modifier.size(88.dp).clip(MochiRoundedRectangle(22.dp)).background(Color.White),
                contentAlignment = Alignment.Center,
            ) {
                val preview = state.previewIcon
                if (preview != null) {
                    Image(preview, contentDescription = null, modifier = Modifier.size(88.dp), contentScale = ContentScale.Crop)
                } else {
                    Image(painterResource(R.drawable.ic_launcher_foreground), contentDescription = null, modifier = Modifier.size(88.dp))
                }
            }
            MochiListCard {
                MochiListRow(stringResource(R.string.module_shortcut_icon_pick), leading = { KsuTile("image", KsuTileColors.Blue) }, onClick = onPickIcon)
                if (state.iconUri != state.defaultShortcutIconUri) {
                    MochiListRow(stringResource(R.string.undo), leading = { KsuTile("arrow-undo") }, onClick = state::resetIconToDefault)
                }
                MochiListRow(
                    stringResource(R.string.module_shortcut_copy_scheme),
                    leading = { KsuTile("link", KsuTileColors.Teal) },
                    onClick = {
                        val url = state.buildShortcutUrl() ?: return@MochiListRow
                        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                        clipboard.setPrimaryClip(ClipData.newPlainText("KernelSU deep link", url))
                        Toast.makeText(context, copied, Toast.LENGTH_SHORT).show()
                    },
                )
                if (state.hasExistingShortcut) {
                    MochiListRow(
                        stringResource(R.string.module_shortcut_delete),
                        leading = { KsuTile("trash", KsuTileColors.Red) },
                        destructive = true,
                        onClick = {
                            onDelete()
                            onDismiss()
                        },
                    )
                }
            }
            MochiTextField(
                value = state.name,
                onValueChange = state::updateName,
                placeholder = stringResource(R.string.module_shortcut_name_label),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
