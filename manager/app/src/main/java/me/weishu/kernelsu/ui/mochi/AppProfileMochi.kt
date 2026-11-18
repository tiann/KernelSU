package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AppIconImage
import me.weishu.kernelsu.ui.component.profile.AppProfileConfig
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.component.profile.TemplateConfig
import me.weishu.kernelsu.ui.screen.appprofile.AppProfileActions
import me.weishu.kernelsu.ui.screen.appprofile.AppProfileUiState
import me.weishu.kernelsu.ui.screen.appprofile.Mode
import me.weishu.kernelsu.ui.util.listAppProfileTemplates
import me.weishu.kernelsu.ui.util.ownerNameForUid
import me.weishu.kernelsu.ui.util.setSepolicy
import me.weishu.kernelsu.ui.viewmodel.getTemplateInfoById
import ui.feice.mochi.basic.MochiBarIconDropdown
import ui.feice.mochi.basic.MochiItem
import ui.feice.mochi.basic.Text
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListPickerRow
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.navigation.MochiBackButton
import ui.feice.mochi.navigation.MochiLazyCompactTitleScaffold
import ui.feice.mochi.theme.MochiTheme

@Composable
fun AppProfileScreenMochi(state: AppProfileUiState, actions: AppProfileActions) {
    val group = state.appGroup
    val primary = group.primary
    val profile = state.profile
    val special = primary.special
    val rootGranted = !special && profile.allowSu
    val userId = state.uid / 100000
    val appId = state.uid % 100000
    val templates = remember { listAppProfileTemplates() }
    var rootMode by rememberSaveable {
        mutableStateOf(
            when {
                profile.rootUseDefault -> Mode.Default
                profile.rootTemplate != null -> Mode.Template
                else -> Mode.Custom
            },
        )
    }
    val nonRootMode = if (profile.nonRootUseDefault) Mode.Default else Mode.Custom
    val showActions = !state.isUidGroup && !primary.isWebViewZygote
    var menu by remember { mutableStateOf(false) }
    val more = stringResource(R.string.mochi_more)

    MochiLazyCompactTitleScaffold(
        title = stringResource(R.string.profile),
        navigationIcon = { MochiBackButton() },
        actions = {
            if (showActions) {
                MochiBarIconDropdown(
                    expanded = menu,
                    onExpandedChange = { menu = it },
                    items = persistentListOf(
                        MochiItem(stringResource(R.string.launch_app), icon = { MochiIonicon("open", tint = it) }),
                        MochiItem(stringResource(R.string.force_stop_app), icon = { MochiIonicon("stop-circle", tint = it) }),
                        MochiItem(stringResource(R.string.restart_app), icon = { MochiIonicon("refresh", tint = it) }),
                    ),
                    onItemClick = { index ->
                        when (index) {
                            0 -> actions.onLaunchApp(state.packageName, userId)
                            1 -> actions.onForceStopApp(state.packageName, userId)
                            else -> actions.onRestartApp(state.packageName, userId)
                        }
                    },
                    modifier = Modifier.semantics { contentDescription = more },
                    navId = "profile.more",
                    contentKey = "ellipsis",
                ) { MochiIonicon("ellipsis-horizontal") }
            }
        },
    ) {
        item(key = "app") {
            Column(Modifier.fillMaxWidth().padding(vertical = 8.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                AppIconImage(packageInfo = primary.packageInfo, label = primary.label, modifier = Modifier.size(72.dp))
                Text(
                    if (state.isUidGroup) ownerNameForUid(primary.uid) else primary.label,
                    modifier = Modifier.padding(top = 12.dp),
                    style = MochiTheme.textStyles.titleLarge.copy(textAlign = TextAlign.Center),
                )
                val detail = when {
                    special -> primary.displayIdentifier
                    state.isUidGroup -> state.sharedUserId.ifEmpty { stringResource(R.string.group_contains_apps, group.apps.size) }
                    else -> "${primary.displayIdentifier}\n${primary.packageInfo.versionName.orEmpty()} (${primary.packageInfo.longVersionCode})"
                }
                Text(
                    detail,
                    style = MochiTheme.textStyles.subhead.copy(textAlign = TextAlign.Center),
                    color = MochiTheme.colorScheme.onSurfaceVariant,
                )
                Row(Modifier.padding(top = 10.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    if (userId != 0) {
                        MochiListStatusChip("USER $userId")
                        MochiListStatusChip("UID $appId")
                    } else {
                        MochiListStatusChip("UID ${state.uid}")
                    }
                }
            }
        }
        if (!special) {
            item(key = "su") {
                MochiListSection {
                    KsuSwitchRow(
                        stringResource(R.string.superuser),
                        rootGranted,
                        { actions.onProfileChange(profile.copy(allowSu = it)) },
                        icon = "shield-checkmark",
                        color = KsuTileColors.Green,
                    )
                }
            }
        }
        item(key = "mode") {
            val modes = buildList {
                add(Mode.Default)
                if (rootGranted) add(Mode.Template)
                add(Mode.Custom)
            }
            val labels = modes.map {
                when (it) {
                    Mode.Default -> stringResource(R.string.profile_default)
                    Mode.Template -> stringResource(R.string.profile_template)
                    Mode.Custom -> stringResource(R.string.profile_custom)
                }
            }
            val current = if (rootGranted) rootMode else nonRootMode
            MochiListSection(header = stringResource(R.string.profile)) {
                MochiListPickerRow(
                    stringResource(if (rootGranted) R.string.superuser else R.string.profile),
                    options = labels.toImmutableList(),
                    selectedIndex = modes.indexOf(current).coerceAtLeast(0),
                    onSelect = { index ->
                        val mode = modes[index]
                        if (!rootGranted) {
                            actions.onProfileChange(profile.copy(nonRootUseDefault = mode == Mode.Default))
                            return@MochiListPickerRow
                        }
                        when (mode) {
                            Mode.Default, Mode.Custom -> {
                                actions.onProfileChange(profile.copy(rootUseDefault = mode == Mode.Default, rootTemplate = null))
                                rootMode = mode
                            }

                            Mode.Template -> if (templates.isNotEmpty()) {
                                val selected = profile.rootTemplate ?: templates[0]
                                val info = getTemplateInfoById(selected)
                                if (info != null && setSepolicy(selected, info.rules.joinToString("\n"))) {
                                    actions.onProfileChange(
                                        profile.copy(
                                            rootUseDefault = false,
                                            rootTemplate = selected,
                                            uid = info.uid,
                                            gid = info.gid,
                                            groups = info.groups,
                                            capabilities = info.capabilities,
                                            context = info.context,
                                            namespace = info.namespace,
                                        ),
                                    )
                                } else if (profile.rootTemplate != selected || profile.rootUseDefault) {
                                    actions.onProfileChange(profile.copy(rootUseDefault = false, rootTemplate = selected))
                                }
                                rootMode = Mode.Template
                            } else {
                                actions.onManageTemplate()
                            }
                        }
                    },
                    leading = { KsuTile("options", KsuTileColors.Indigo) },
                )
            }
        }
        when {
            rootGranted && rootMode == Mode.Template -> item(key = "template") {
                MochiListSection {
                    TemplateConfig(
                        profile = profile,
                        onViewTemplate = actions.onViewTemplate,
                        onManageTemplate = actions.onManageTemplate,
                        onProfileChange = actions.onProfileChange,
                    )
                }
            }

            rootGranted && rootMode == Mode.Custom -> item(key = "root") {
                MochiListSection {
                    RootProfileConfig(fixedName = true, enabled = true, profile = profile, onProfileChange = actions.onProfileChange)
                }
            }

            !rootGranted && nonRootMode == Mode.Custom -> item(key = "nonroot") {
                MochiListSection {
                    AppProfileConfig(fixedName = true, enabled = true, profile = profile, onProfileChange = actions.onProfileChange)
                }
            }
        }
        if (state.isUidGroup) {
            item(key = "apps") {
                MochiListSection(header = stringResource(R.string.app_profile_affects_following_apps)) {
                    group.apps.forEach { app ->
                        MochiListRow(
                            app.label,
                            subtitle = app.displayIdentifier,
                            leading = { AppIconImage(packageInfo = app.packageInfo, label = app.label, modifier = Modifier.size(28.dp)) },
                        )
                    }
                }
            }
        }
    }
}
