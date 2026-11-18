package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import kotlinx.coroutines.delay
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.script.ScriptParam
import me.weishu.kernelsu.ui.screen.executemoduleaction.ActionResultKind
import me.weishu.kernelsu.ui.screen.executemoduleaction.ExecuteModuleActionScreenActions
import me.weishu.kernelsu.ui.screen.executemoduleaction.ExecuteModuleActionUiState
import me.weishu.kernelsu.ui.screen.flash.FlashScreenActions
import me.weishu.kernelsu.ui.screen.flash.FlashUiState
import me.weishu.kernelsu.ui.screen.flash.FlashingStatus
import me.weishu.kernelsu.ui.screen.scriptrun.ScriptRunActions
import me.weishu.kernelsu.ui.screen.scriptrun.ScriptRunPhase
import me.weishu.kernelsu.ui.screen.scriptrun.ScriptRunUiState
import me.weishu.kernelsu.ui.screen.scriptrun.integerKeyboardType
import me.weishu.kernelsu.ui.screen.scriptrun.lastRunText
import me.weishu.kernelsu.ui.screen.scriptrun.paramErrorText
import me.weishu.kernelsu.ui.screen.scriptrun.scriptSubtitle
import me.weishu.kernelsu.ui.util.ShellHighlightColors
import me.weishu.kernelsu.ui.util.rememberShellHighlight
import ui.feice.mochi.basic.MochiActivityIndicator
import ui.feice.mochi.basic.MochiBarIconButton
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiEmptyState
import ui.feice.mochi.basic.MochiSwitch
import ui.feice.mochi.basic.MochiTextField
import ui.feice.mochi.basic.Text
import ui.feice.mochi.effect.content.MochiBlurText
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListDefaults
import ui.feice.mochi.list.MochiListDisclosure
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.MochiListSectionFooter
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold
import ui.feice.mochi.theme.MochiTheme

/**
 * Shared log screen for flashing, module actions and scripts. Scrolls to the bottom on new output,
 * with a save button on top and an action button at the bottom.
 */
@Composable
private fun LogPage(
    title: String,
    subtitle: String?,
    text: String,
    onSaveLog: () -> Unit,
    canGoBack: Boolean,
    footer: (@Composable () -> Unit)? = null,
    bottomAction: (@Composable () -> Unit)? = null,
) {
    val listState = androidx.compose.foundation.lazy.rememberLazyListState()
    val saveLabel = stringResource(R.string.save_log)
    LaunchedEffect(text) {
        val last = listState.layoutInfo.totalItemsCount - 1
        if (text.isNotEmpty() && last >= 0) listState.animateScrollToItem(last)
    }
    Box(Modifier.fillMaxSize()) {
        MochiLazyCompactTitleScaffold(
            title = title,
            subtitle = subtitle,
            listState = listState,
            navigationIcon = if (canGoBack) {
                { MochiBackButton() }
            } else {
                null
            },
            actions = {
                MochiBarIconButton(
                    onClick = onSaveLog,
                    modifier = Modifier.semantics { contentDescription = saveLabel },
                    navId = "log.save",
                    contentKey = "save",
                ) { MochiIonicon("save") }
            },
        ) {
            item(key = "log") {
                MochiListCard(animateHeight = false) {
                    SelectionContainer {
                        Text(
                            text.ifEmpty { " " },
                            modifier = Modifier
                                .fillMaxWidth()
                                .horizontalScroll(rememberScrollState())
                                .padding(horizontal = MochiListDefaults.RowPadding, vertical = 14.dp),
                            style = MochiTheme.textStyles.footnote.copy(fontFamily = FontFamily.Monospace),
                            softWrap = false,
                        )
                    }
                }
            }
            footer?.let { item(key = "footer") { it() } }
            if (bottomAction != null) item(key = "space") { Box(Modifier.padding(bottom = 72.dp)) }
        }
        if (bottomAction != null) {
            Box(Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 16.dp)) { bottomAction() }
        }
    }
}

@Composable
fun FlashScreenMochi(state: FlashUiState, actions: FlashScreenActions) {
    if (state.showJailbreakWarning) {
        var countdown by remember { mutableIntStateOf(10) }
        LaunchedEffect(Unit) {
            while (countdown > 0) {
                delay(1000)
                countdown--
            }
        }
        MochiDialog(
            visible = true,
            onDismissRequest = actions.onDismissJailbreakWarning,
            title = stringResource(android.R.string.dialog_alert_title),
            message = stringResource(R.string.jailbreak_flash_warning),
            icon = { MochiIonicon("warning", tint = MochiTheme.colorScheme.error, size = 44.dp) },
            actions = persistentListOf(
                MochiDialogAction(stringResource(android.R.string.cancel), MochiDialogActionRole.Cancel, onClick = actions.onDismissJailbreakWarning),
                MochiDialogAction(
                    if (countdown > 0) stringResource(R.string.jailbreak_flash_warning_countdown, countdown) else stringResource(R.string.install_next),
                    MochiDialogActionRole.Destructive,
                    enabled = countdown == 0,
                    onClick = actions.onConfirmJailbreakWarning,
                ),
            ),
        )
    }
    val title = stringResource(
        when (state.flashingStatus) {
            FlashingStatus.FLASHING -> R.string.flashing
            FlashingStatus.SUCCESS -> R.string.flash_success
            FlashingStatus.FAILED -> R.string.flash_failed
        },
    )
    LogPage(
        title = title,
        subtitle = null,
        text = state.text,
        onSaveLog = actions.onSaveLog,
        canGoBack = state.flashingStatus != FlashingStatus.FLASHING,
        footer = if (state.flashingStatus == FlashingStatus.FLASHING) {
            { Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) { MochiActivityIndicator(size = 24.dp) } }
        } else {
            null
        },
        bottomAction = if (state.showRebootAction) {
            {
                MochiButton(onClick = actions.onReboot, style = MochiButtonStyle.ProminentGlass) {
                    MochiIonicon("power", size = 18.dp)
                    Text(stringResource(state.rebootLabelRes), modifier = Modifier.padding(start = 6.dp))
                }
            }
        } else {
            null
        },
    )
}

@Composable
fun ExecuteModuleActionScreenMochi(state: ExecuteModuleActionUiState, actions: ExecuteModuleActionScreenActions) {
    androidx.activity.compose.BackHandler(enabled = !state.isComplete) { }
    val cs = MochiTheme.colorScheme
    LogPage(
        title = state.title ?: stringResource(R.string.action),
        subtitle = null,
        text = state.text,
        onSaveLog = actions.onSaveLog,
        canGoBack = state.isComplete,
        footer = when {
            state.resultNotice != null -> {
                {
                    val notice = state.resultNotice
                    val (icon, tint) = when (notice.kind) {
                        ActionResultKind.SUCCESS -> "checkmark-circle" to cs.primary
                        ActionResultKind.NOT_APPLICABLE -> "information-circle" to cs.onSurfaceVariant
                        ActionResultKind.FAILURE -> "close-circle" to cs.error
                    }
                    MochiListCard {
                        MochiListRow(
                            notice.text,
                            leading = { MochiIonicon(icon, tint = tint) },
                            leadingStyle = ui.feice.mochi.list.MochiListLeadingStyle.Symbol,
                        )
                    }
                }
            }

            !state.isComplete -> {
                { Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) { MochiActivityIndicator(size = 24.dp) } }
            }

            else -> null
        },
        bottomAction = if (state.isComplete) {
            {
                MochiButton(onClick = actions.onClose, style = MochiButtonStyle.ProminentGlass) {
                    MochiIonicon("checkmark", size = 18.dp)
                    Text(stringResource(R.string.close), modifier = Modifier.padding(start = 6.dp))
                }
            }
        } else {
            null
        },
    )
}

@Composable
fun ScriptRunScreenMochi(state: ScriptRunUiState, actions: ScriptRunActions) {
    val context = LocalContext.current
    val cs = MochiTheme.colorScheme
    val prepared = state.prepared
    Box(Modifier.fillMaxSize()) {
        MochiLazyCompactTitleScaffold(
            title = stringResource(R.string.script_run_title),
            subtitle = prepared?.manifest?.name ?: state.displayName,
            navigationIcon = { MochiBackButton() },
        ) {
            when {
                prepared == null && state.phase != ScriptRunPhase.FAILED -> item(key = "loading") {
                    MochiEmptyState(stringResource(R.string.script_preparing), icon = { MochiActivityIndicator(size = 28.dp) })
                }

                prepared == null -> {
                    item(key = "error") {
                        MochiEmptyState(
                            stringResource(R.string.script_error_title),
                            message = state.error,
                            icon = { MochiIonicon("warning", size = 44.dp, tint = cs.error) },
                        )
                    }
                    if (state.offerInstall) item(key = "install") { InstallAnyway(actions) }
                }

                else -> {
                    val manifest = prepared.manifest
                    item(key = "info") {
                        MochiListSection {
                            MochiListRow(
                                manifest.name,
                                subtitle = buildString {
                                    append(scriptSubtitle(manifest.id, manifest.version, manifest.versionCode))
                                    if (manifest.author.isNotBlank()) {
                                        append('\n').append(stringResource(R.string.module_author)).append(": ").append(manifest.author)
                                    }
                                    if (manifest.description.isNotBlank()) append('\n').append(manifest.description)
                                },
                                leading = { KsuTile("code-slash", KsuTileColors.Indigo) },
                            )
                            MochiListRow(stringResource(R.string.script_notice), subtitle = lastRunText(context, state.lastRun))
                        }
                    }
                    if (state.offerInstall) item(key = "install") { InstallAnyway(actions) }
                    val params = prepared.params.params
                    if (params.isNotEmpty()) {
                        item(key = "params") {
                            MochiListSection(header = stringResource(R.string.script_params)) {
                                params.forEach { param -> ParamRow(param, state, actions) }
                            }
                        }
                    }
                    item(key = "source") {
                        var expanded by rememberSaveable { mutableStateOf(false) }
                        MochiListCard {
                            MochiListDisclosure(
                                stringResource(R.string.script_source),
                                expanded = expanded,
                                onExpandedChange = { expanded = it },
                                subtitle = if (prepared.actionSourceTruncated) stringResource(R.string.script_source_truncated) else null,
                            ) {
                                val source = rememberShellHighlight(prepared.actionSource, ShellHighlightColors.forBackground(cs.surfaceContainer))
                                SelectionContainer {
                                    BasicText(
                                        source,
                                        modifier = Modifier
                                            .horizontalScroll(rememberScrollState())
                                            .padding(horizontal = MochiListDefaults.RowPadding, vertical = 12.dp),
                                        style = MochiTheme.textStyles.footnote.copy(fontFamily = FontFamily.Monospace, color = cs.onSurface),
                                        softWrap = false,
                                    )
                                }
                            }
                        }
                    }
                    item(key = "space") { Box(Modifier.padding(bottom = 72.dp)) }
                }
            }
        }
        if (prepared != null) {
            Box(Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 16.dp)) {
                MochiButton(onClick = actions.onRun, enabled = state.canRun, style = MochiButtonStyle.ProminentGlass) {
                    MochiIonicon("play", size = 18.dp)
                    Text(stringResource(R.string.script_run), modifier = Modifier.padding(start = 6.dp))
                }
            }
        }
    }
}

@Composable
private fun InstallAnyway(actions: ScriptRunActions) {
    MochiListSection(footer = stringResource(R.string.script_local_notice)) {
        MochiListRow(
            stringResource(R.string.script_install_anyway),
            leading = { KsuTile("download", KsuTileColors.Blue) },
            chevron = true,
            onClick = actions.onInstallAsModule,
        )
    }
}

@Composable
private fun ParamRow(param: ScriptParam, state: ScriptRunUiState, actions: ScriptRunActions) {
    val value = state.values[param.key] ?: param.defaultValue
    val error = state.errors[param.key]?.let { paramErrorText(param, it) }
    when (param) {
        is ScriptParam.Bool -> MochiListRow(
            param.label,
            subtitle = param.description,
            trailing = { MochiSwitch(value.toBoolean(), { actions.onValueChange(param.key, it.toString()) }) },
        )

        is ScriptParam.Choice -> MochiListPickerRow(
            param.label,
            options = param.options.map { it.label ?: it.value }.toImmutableList(),
            selectedIndex = param.options.indexOfFirst { it.value == value }.coerceAtLeast(0),
            onSelect = { actions.onValueChange(param.key, param.options[it].value) },
        )

        is ScriptParam.Text, is ScriptParam.Integer -> Column(
            Modifier.fillMaxWidth().padding(horizontal = MochiListDefaults.RowPadding, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(param.label, style = MochiTheme.textStyles.body)
            MochiTextField(
                value = value,
                onValueChange = { actions.onValueChange(param.key, it) },
                placeholder = param.description.orEmpty(),
                error = error,
                keyboardOptions = KeyboardOptions(
                    keyboardType = if (param is ScriptParam.Integer) integerKeyboardType(param) else KeyboardType.Text,
                ),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
