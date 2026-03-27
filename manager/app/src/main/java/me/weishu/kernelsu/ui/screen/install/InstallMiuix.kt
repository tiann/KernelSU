package me.weishu.kernelsu.ui.screen.install

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.selection.toggleable
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.util.LkmSelection
import me.weishu.kernelsu.ui.util.defaultHazeEffect
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.extra.SuperCheckbox
import top.yukonga.miuix.kmp.extra.SuperDropdown
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.basic.ArrowRight
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.icon.extended.Close
import top.yukonga.miuix.kmp.icon.extended.ConvertFile
import top.yukonga.miuix.kmp.icon.extended.ExpandLess
import top.yukonga.miuix.kmp.icon.extended.ExpandMore
import top.yukonga.miuix.kmp.icon.extended.MoveFile
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

/**
 * @author weishu
 * @date 2024/3/12.
 */
@Composable
internal fun InstallScreenMiuix(
    uiState: InstallUiState,
    actions: InstallScreenActions,
) {
    val enableBlur = LocalEnableBlur.current
    val scrollBehavior = MiuixScrollBehavior()
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
            TopBar(
                onBack = actions.onBack,
                scrollBehavior = scrollBehavior,
                hazeState = hazeState,
                hazeStyle = hazeStyle,
                enableBlur = enableBlur,
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxHeight()
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .let { if (enableBlur) it.hazeSource(state = hazeState) else it }
                .padding(top = 12.dp)
                .padding(horizontal = 16.dp),
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    SelectInstallMethod(
                        state = uiState,
                        onSelected = actions.onSelectMethod,
                        onSelectBootImage = actions.onSelectBootImage,
                    )
                }
                AnimatedVisibility(
                    visible = uiState.canSelectPartition,
                    enter = expandVertically(),
                    exit = shrinkVertically()
                ) {
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 12.dp),
                    ) {
                        SuperDropdown(
                            items = uiState.displayPartitions,
                            selectedIndex = uiState.partitionSelectionIndex,
                            title = "${stringResource(R.string.install_select_partition)} (${uiState.slotSuffix})",
                            onSelectedIndexChange = actions.onSelectPartition,
                            startAction = {
                                Icon(
                                    MiuixIcons.ConvertFile,
                                    tint = colorScheme.onSurface,
                                    modifier = Modifier.padding(end = 12.dp),
                                    contentDescription = null
                                )
                            }
                        )
                    }
                }
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 12.dp),
                ) {
                    BasicComponent(
                        title = stringResource(id = R.string.install_upload_lkm_file),
                        summary = (uiState.lkmSelection as? LkmSelection.LkmUri)?.let {
                            stringResource(id = R.string.selected_lkm, it.uri.lastPathSegment ?: "(file)")
                        },
                        onClick = actions.onUploadLkm,
                        startAction = {
                            Icon(
                                MiuixIcons.MoveFile,
                                tint = colorScheme.onSurface,
                                modifier = Modifier.padding(end = 12.dp),
                                contentDescription = null
                            )
                        },
                        endActions = {
                            if (uiState.lkmSelection is LkmSelection.LkmUri) {
                                IconButton(onClick = actions.onClearLkm) {
                                    Icon(
                                        MiuixIcons.Close,
                                        modifier = Modifier.size(16.dp),
                                        contentDescription = stringResource(android.R.string.cancel),
                                        tint = colorScheme.onSurfaceVariantActions
                                    )
                                }
                            } else {
                                val layoutDirection = LocalLayoutDirection.current
                                Icon(
                                    modifier = Modifier
                                        .size(width = 10.dp, height = 16.dp)
                                        .graphicsLayer {
                                            scaleX = if (layoutDirection == LayoutDirection.Rtl) -1f else 1f
                                        }
                                        .align(Alignment.CenterVertically),
                                    imageVector = MiuixIcons.Basic.ArrowRight,
                                    contentDescription = null,
                                    tint = colorScheme.onSurfaceVariantActions,
                                )
                            }
                        }
                    )
                }
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 12.dp),
                ) {
                    BasicComponent(
                        title = stringResource(id = R.string.advanced_options),
                        onClick = actions.onAdvancedOptionsClicked,
                        endActions = {
                            Icon(
                                if (uiState.advancedOptionsShown) MiuixIcons.ExpandLess else MiuixIcons.ExpandMore,
                                modifier = Modifier.size(16.dp),
                                tint = colorScheme.onSurfaceVariantActions,
                                contentDescription = stringResource(R.string.expand),
                            )
                        }
                    )
                    AnimatedVisibility(
                        visible = uiState.advancedOptionsShown,
                        enter = expandVertically() + fadeIn(),
                        exit = shrinkVertically() + fadeOut()
                    ) {
                        Column {
                            SuperCheckbox(
                                title = stringResource(id = R.string.allow_shell),
                                checked = uiState.allowShell,
                                summary = stringResource(id = R.string.allow_shell_summary),
                                onCheckedChange = actions.onSelectAllowShell
                            )
                            SuperCheckbox(
                                title = stringResource(id = R.string.enable_adb),
                                checked = uiState.enableAdb,
                                summary = stringResource(id = R.string.enable_adb_summary),
                                onCheckedChange = actions.onSelectEnableAdb
                            )
                        }
                    }
                }
                TextButton(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 12.dp),
                    text = stringResource(id = R.string.install_next),
                    enabled = uiState.installMethod != null,
                    colors = ButtonDefaults.textButtonColorsPrimary(),
                    onClick = actions.onNext
                )
                Spacer(
                    Modifier.height(
                        WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                WindowInsets.captionBar.asPaddingValues().calculateBottomPadding()
                    )
                )
            }
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
        }
    )
    val dialogTitle = stringResource(id = android.R.string.dialog_alert_title)
    val dialogContent = stringResource(id = R.string.install_inactive_slot_warning)

    val onClick = { option: InstallMethod ->
        when (option) {
            is InstallMethod.SelectFile -> onSelectBootImage()
            is InstallMethod.DirectInstall -> onSelected(option)
            is InstallMethod.DirectInstallToInactiveSlot -> confirmDialog.showConfirm(dialogTitle, dialogContent)
        }
    }

    Column {
        state.installMethodOptions.forEach { option ->
            val interactionSource = remember { MutableInteractionSource() }
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .fillMaxWidth()
                    .toggleable(
                        value = option.javaClass == state.installMethod?.javaClass,
                        onValueChange = { onClick(option) },
                        role = Role.RadioButton,
                        indication = LocalIndication.current,
                        interactionSource = interactionSource
                    )
            ) {
                SuperCheckbox(
                    title = stringResource(id = option.label),
                    summary = option.summary,
                    checked = option.javaClass == state.installMethod?.javaClass,
                    onCheckedChange = { onClick(option) },
                )
            }
        }
    }
}

@Composable
private fun TopBar(
    onBack: () -> Unit = {},
    scrollBehavior: ScrollBehavior,
    hazeState: HazeState,
    hazeStyle: HazeStyle,
    enableBlur: Boolean
) {
    TopAppBar(
        modifier = if (enableBlur) {
            Modifier.defaultHazeEffect(hazeState, hazeStyle)
        } else {
            Modifier
        },
        color = if (enableBlur) Color.Transparent else colorScheme.surface,
        title = stringResource(R.string.install),
        navigationIcon = {
            IconButton(
                modifier = Modifier.padding(start = 16.dp),
                onClick = onBack
            ) {
                val layoutDirection = LocalLayoutDirection.current
                Icon(
                    modifier = Modifier.graphicsLayer {
                        if (layoutDirection == LayoutDirection.Rtl) scaleX = -1f
                    },
                    imageVector = MiuixIcons.Back,
                    tint = colorScheme.onSurface,
                    contentDescription = null,
                )
            }
        },
        scrollBehavior = scrollBehavior
    )
}
