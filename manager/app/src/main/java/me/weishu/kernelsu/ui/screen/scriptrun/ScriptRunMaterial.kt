package me.weishu.kernelsu.ui.screen.scriptrun

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.LoadingIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.script.PreparedScript
import me.weishu.kernelsu.data.script.ScriptParam
import me.weishu.kernelsu.ui.component.material.ExpressiveScaffold
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedDropdownItem
import me.weishu.kernelsu.ui.component.material.SegmentedItemContainer
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.SegmentedSwitchItem
import me.weishu.kernelsu.ui.component.material.SegmentedTextField
import me.weishu.kernelsu.ui.component.material.TonalCard
import me.weishu.kernelsu.ui.component.material.TopBarBackButton
import me.weishu.kernelsu.ui.component.material.expressiveTopAppBarColors
import me.weishu.kernelsu.ui.util.ShellHighlightColors
import me.weishu.kernelsu.ui.util.rememberShellHighlight

@Composable
internal fun ScriptRunScreenMaterial(
    state: ScriptRunUiState,
    actions: ScriptRunActions,
) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())

    ExpressiveScaffold(
        topBar = { TopBar(onBack = actions.onBack, scrollBehavior = scrollBehavior) },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val prepared = state.prepared
        when {
            state.phase == ScriptRunPhase.FAILED -> Column(
                modifier = Modifier.padding(innerPadding),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text(
                        text = stringResource(R.string.script_error_title),
                        style = MaterialTheme.typography.titleMedium,
                        color = MaterialTheme.colorScheme.error,
                    )
                    Text(text = state.error.orEmpty(), style = MaterialTheme.typography.bodyMedium)
                }
                if (state.offerInstall) InstallAnywayCard(actions)
            }

            prepared == null -> Box(
                modifier = Modifier
                    .padding(innerPadding)
                    .fillMaxSize(),
                contentAlignment = Alignment.Center,
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    LoadingIndicator()
                    Text(
                        text = stringResource(R.string.script_preparing),
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.padding(top = 8.dp),
                    )
                }
            }

            else -> Column(
                modifier = Modifier
                    .padding(innerPadding)
                    .fillMaxSize()
                    .imePadding()
                    .nestedScroll(scrollBehavior.nestedScrollConnection)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(13.dp)
            ) {
                ScriptInfo(state, prepared, actions)
                ParamsForm(state, prepared, actions)
                SourceSection(prepared)
                Button(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp),
                    enabled = state.canRun,
                    onClick = actions.onRun,
                ) {
                    Icon(Icons.Filled.PlayArrow, contentDescription = null)
                    Text(stringResource(R.string.script_run), modifier = Modifier.padding(start = 8.dp))
                }
                Spacer(Modifier.height(16.dp + WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding()))
            }
        }
    }
}

@Composable
private fun ScriptInfo(state: ScriptRunUiState, prepared: PreparedScript, actions: ScriptRunActions) {
    val context = LocalContext.current
    val manifest = prepared.manifest
    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = buildList {
            add {
                SegmentedListItem(
                    headlineContent = { Text(manifest.name) },
                    supportingContent = {
                        Column {
                            Text(scriptSubtitle(manifest.id, manifest.version, manifest.versionCode))
                            if (manifest.author.isNotBlank()) {
                                Text("${stringResource(R.string.module_author)}: ${manifest.author}")
                            }
                        }
                    },
                )
            }
            if (manifest.description.isNotBlank()) add {
                SegmentedListItem(headlineContent = { Text(manifest.description, style = MaterialTheme.typography.bodyMedium) })
            }
            add {
                SegmentedListItem(
                    headlineContent = {
                        Text(stringResource(R.string.script_notice), style = MaterialTheme.typography.bodyMedium)
                    },
                    supportingContent = { Text(lastRunText(context, state.lastRun)) },
                )
            }
        }
    )
    if (state.offerInstall) InstallAnywayCard(actions)
}

/** A local ZIP can still be installed as a module, also when it cannot be run as a script. */
@Composable
private fun InstallAnywayCard(actions: ScriptRunActions) {
    TonalCard(modifier = Modifier
        .fillMaxWidth()
        .padding(horizontal = 16.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(stringResource(R.string.script_local_notice), style = MaterialTheme.typography.bodyMedium)
            OutlinedButton(
                onClick = actions.onInstallAsModule,
                modifier = Modifier.padding(top = 8.dp),
            ) {
                Text(stringResource(R.string.script_install_anyway))
            }
        }
    }
}

@Composable
private fun ParamsForm(state: ScriptRunUiState, prepared: PreparedScript, actions: ScriptRunActions) {
    val params = prepared.params.params
    if (params.isEmpty()) return
    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        title = stringResource(R.string.script_params),
        content = params.map { param ->
            {
                val value = state.values[param.key] ?: param.defaultValue
                val error = state.errors[param.key]
                when (param) {
                    is ScriptParam.Bool -> SegmentedSwitchItem(
                        title = param.label,
                        summary = param.description,
                        checked = value == "true",
                        onCheckedChange = { actions.onValueChange(param.key, it.toString()) },
                    )

                    is ScriptParam.Choice -> SegmentedDropdownItem(
                        title = param.label,
                        summary = param.description,
                        items = param.options.map { it.label ?: it.value },
                        selectedIndex = param.options.indexOfFirst { it.value == value }.coerceAtLeast(0),
                        onItemSelected = { index -> actions.onValueChange(param.key, param.options[index].value) },
                    )

                    is ScriptParam.Text, is ScriptParam.Integer -> {
                        val errorText = error?.let { paramErrorText(param, it) }
                        SegmentedTextField(
                            label = param.label,
                            value = value,
                            onValueChange = { actions.onValueChange(param.key, it) },
                            singleLine = true,
                            isError = error != null,
                            placeholder = null,
                            keyboardOptions = if (param is ScriptParam.Integer) {
                                KeyboardOptions(keyboardType = integerKeyboardType(param))
                            } else {
                                KeyboardOptions.Default
                            },
                            supportingContent = (errorText ?: param.description)?.let { text ->
                                {
                                    Text(
                                        text = text,
                                        color = if (errorText != null) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            },
                        )
                    }
                }
            }
        }
    )
}

@Composable
private fun SourceSection(prepared: PreparedScript) {
    var expanded by rememberSaveable { mutableStateOf(false) }
    val rotation by animateFloatAsState(targetValue = if (expanded) 180f else 0f, label = "SourceExpand")
    SegmentedColumn(
        modifier = Modifier.padding(horizontal = 16.dp),
        content = buildList {
            add {
                SegmentedListItem(
                    headlineContent = { Text(stringResource(R.string.script_source)) },
                    supportingContent = if (prepared.actionSourceTruncated) {
                        { Text(stringResource(R.string.script_source_truncated)) }
                    } else {
                        null
                    },
                    trailingContent = {
                        Icon(
                            imageVector = Icons.Filled.ExpandMore,
                            contentDescription = stringResource(R.string.expand),
                            modifier = Modifier.graphicsLayer { rotationZ = rotation }
                        )
                    },
                    onClick = { expanded = !expanded },
                )
            }
            if (expanded) add {
                SegmentedItemContainer {
                    val source = rememberShellHighlight(
                        prepared.actionSource,
                        ShellHighlightColors.forBackground(MaterialTheme.colorScheme.surfaceBright),
                    )
                    SelectionContainer {
                        Text(
                            text = source,
                            fontFamily = FontFamily.Monospace,
                            style = MaterialTheme.typography.bodySmall,
                            softWrap = false,
                            modifier = Modifier
                                .horizontalScroll(rememberScrollState())
                                .padding(16.dp),
                        )
                    }
                }
            }
        }
    )
}

@Composable
private fun TopBar(
    onBack: () -> Unit,
    scrollBehavior: TopAppBarScrollBehavior,
) {
    LargeFlexibleTopAppBar(
        title = { Text(stringResource(R.string.script_run_title)) },
        navigationIcon = { TopBarBackButton(onClick = onBack) },
        colors = expressiveTopAppBarColors(),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}
