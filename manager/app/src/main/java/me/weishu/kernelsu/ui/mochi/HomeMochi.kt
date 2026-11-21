package me.weishu.kernelsu.ui.mochi

import androidx.compose.foundation.background
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.layout.layout
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import ui.feice.mochi.effect.elastic.mochiPressDeform
import kotlinx.coroutines.launch
import androidx.compose.ui.unit.sp
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.foundation.shape.CircleShape
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rebootlistpopup.RebootListPopup
import me.weishu.kernelsu.ui.screen.home.HomeActions
import me.weishu.kernelsu.ui.screen.home.HomeUiState
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import ui.feice.mochi.basic.MochiButton
import ui.feice.mochi.basic.MochiButtonGroup
import ui.feice.mochi.basic.MochiButtonStyle
import ui.feice.mochi.basic.Text
import ui.feice.mochi.effect.content.MochiCounterText
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListDefaults
import ui.feice.mochi.list.MochiListLeadingStyle
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListSection
import ui.feice.mochi.list.MochiListSectionHeader
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.navigation.MochiLazyLargeTitleScaffold
import ui.feice.mochi.shape.MochiRoundedRectangle
import ui.feice.mochi.theme.MochiDefaults
import ui.feice.mochi.theme.MochiTheme

private val HeroShape = MochiRoundedRectangle(MochiListDefaults.SectionCorner)

@Composable
fun HomePagerMochi(state: HomeUiState, actions: HomeActions) {
    MochiLazyLargeTitleScaffold(
        title = stringResource(R.string.app_name),
        actions = { RebootListPopup() },
    ) {
        item(key = "status") {
            // Notices and stats belong to the status group, so they use the smaller gap.
            Column {
                StatusCard(state, actions)
                Notices(state, actions, Modifier.gapAboveIfNotEmpty(GroupGap))
                if (state.ksuVersion != null && Natives.isFullFeatured()) {
                    Box(Modifier.padding(top = GroupGap)) { Stats() }
                }
            }
        }
        item(key = "info") { SystemInfoSection(state) }
        item(key = "support") { SupportSection(actions) }
        item(key = "credit") { MochiCredit { actions.onOpenUrl(MochiProjectUrl) } }
    }
}

@Composable
private fun MochiCredit(onClick: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .clickable(remember { MutableInteractionSource() }, indication = null, onClick = onClick)
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Image(painterResource(R.drawable.ic_mochi_dango), contentDescription = null, modifier = Modifier.size(16.dp))
        Text(
            stringResource(R.string.mochi_about_project_summary),
            style = MochiTheme.textStyles.footnote,
            color = MochiTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun StatusCard(state: HomeUiState, actions: HomeActions) {
    val onClick = { if (!state.isLateLoadMode) actions.onInstallClick() }
    when {
        state.ksuVersion != null -> ActiveCard(state, onClick)
        else -> InactiveCard(state, actions, onClick)
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ActiveCard(state: HomeUiState, onClick: () -> Unit) {
    val scheme = MochiTheme.colorScheme
    val mode = when (state.lkmMode) {
        null -> null
        true -> "LKM"
        else -> "GKI"
    }
    val ok = rememberKsuTone(KsuTileColors.Green).content
    val neutralBg = scheme.onSurface.copy(alpha = 0.06f)
    Row(
        Modifier
            .fillMaxWidth()
            .clip(HeroShape)
            .background(MochiDefaults.groupedContainer())
            .clickable(remember { MutableInteractionSource() }, indication = null, enabled = !state.isLateLoadMode, onClick = onClick)
            .padding(horizontal = 18.dp, vertical = 18.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Box(
            Modifier.size(48.dp).clip(CircleShape).background(ok.copy(alpha = 0.12f)),
            contentAlignment = Alignment.Center,
        ) {
            MochiIonicon("checkmark", tint = ok, size = 26.dp)
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(
                stringResource(R.string.home_working),
                style = MochiTheme.textStyles.body.copy(fontSize = 22.sp, lineHeight = 28.sp, fontWeight = FontWeight.Bold),
            )
            Text(
                stringResource(R.string.home_working_version, "${state.ksuVersion}-${state.kernelUAPIVersion}"),
                style = MochiTheme.textStyles.footnote,
                color = scheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            val chips = mode != null || state.showCustomLkmBadge || state.isSafeMode || state.isLateLoadMode
            if (chips) {
                FlowRow(
                    Modifier.padding(top = 6.dp),
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    mode?.let { MochiListStatusChip(it, containerColor = neutralBg, contentColor = scheme.onSurfaceVariant) }
                    if (state.showCustomLkmBadge) {
                        MochiListStatusChip(stringResource(R.string.home_lkm_custom), containerColor = neutralBg, contentColor = scheme.onSurfaceVariant)
                    }
                    if (state.isSafeMode) {
                        MochiListStatusChip(stringResource(R.string.safe_mode), containerColor = scheme.error.copy(alpha = 0.14f), contentColor = scheme.error)
                    }
                    if (state.isLateLoadMode) {
                        MochiListStatusChip(stringResource(R.string.jailbreak_mode), containerColor = scheme.error.copy(alpha = 0.14f), contentColor = scheme.error)
                    }
                }
            }
        }
    }
}

@Composable
private fun InactiveCard(state: HomeUiState, actions: HomeActions, onClick: () -> Unit) {
    val gki = state.kernelVersion.isGKI()
    val scheme = MochiTheme.colorScheme
    Column(
        Modifier
            .fillMaxWidth()
            .clip(HeroShape)
            .background(MochiDefaults.groupedContainer())
            .clickable(remember { MutableInteractionSource() }, indication = null, enabled = !state.isLateLoadMode, onClick = onClick)
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            MochiIonicon(if (gki) "alert-circle" else "close-circle", tint = scheme.error, size = 18.dp)
            Text(
                stringResource(if (gki) R.string.home_click_to_install else R.string.home_unsupported_reason),
                style = MochiTheme.textStyles.subhead,
                color = scheme.error,
            )
        }
        Text(
            stringResource(if (gki) R.string.home_not_installed else R.string.home_unsupported),
            style = MochiTheme.textStyles.largeTitle,
        )
        if (gki) {
            MochiButtonGroup {
                MochiButton(onClick = actions.onInstallClick, enabled = !state.isLateLoadMode, style = MochiButtonStyle.ProminentGlass) {
                    Text(stringResource(R.string.install))
                }
                if (state.isSELinuxPermissive) {
                    MochiButton(onClick = actions.onJailbreakClick, style = MochiButtonStyle.Glass) {
                        Text(stringResource(R.string.home_jailbreak))
                    }
                }
            }
        }
    }
}

@Composable
private fun Notice(message: String, error: Boolean, onClick: (() -> Unit)? = null) {
    val scheme = MochiTheme.colorScheme
    MochiListCard {
        MochiListRow(
            message,
            leading = {
                MochiIonicon(
                    if (error) "alert-circle" else "notifications",
                    tint = if (error) scheme.error else scheme.onSurfaceVariant,
                )
            },
            leadingStyle = MochiListLeadingStyle.Symbol,
            chevron = onClick != null,
            onClick = onClick,
        )
    }
}

@Composable
private fun Notices(state: HomeUiState, actions: HomeActions, modifier: Modifier = Modifier) {
    val release = state.latestVersionInfo
    val changelogTitle = stringResource(R.string.module_changelog)
    val updateText = stringResource(R.string.module_update)
    val updateDialog = rememberConfirmDialog(onConfirm = { actions.onOpenUrl(release.downloadUrl) })
    // Parse the changelog ahead of time so the dialog does not stall on open.
    val linkColor = MochiTheme.colorScheme.primary
    LaunchedEffect(release.changelog, linkColor) { prewarmMochiRichText(release.changelog, true, linkColor) }
    Column(modifier, verticalArrangement = Arrangement.spacedBy(12.dp)) {
        if (state.checkUpdateEnabled && state.hasUpdate) {
            Notice(stringResource(R.string.new_version_available, release.versionCode), error = false, onClick = {
                if (release.changelog.isEmpty()) {
                    actions.onOpenUrl(release.downloadUrl)
                } else {
                    updateDialog.showConfirm(title = changelogTitle, content = release.changelog, markdown = true, confirm = updateText)
                }
            })
        }
        if (state.showManagerPrBuildWarning) {
            Notice(stringResource(R.string.home_pr_build_warning), error = false)
        } else if (state.showKernelPrBuildWarning) {
            Notice(stringResource(R.string.home_pr_kernel_warning), error = false)
        }
        if (state.showGkiWarning) Notice(stringResource(R.string.home_gki_warning), error = false)
        if (state.requiresNewKernel) {
            Notice(
                stringResource(if (state.canInstallKernelUpdate) R.string.require_kernel_version else R.string.require_kernel_version_gki),
                error = true,
                onClick = if (state.canInstallKernelUpdate) actions.onInstallClick else null,
            )
        }
        if (state.requiresNewManager) Notice(stringResource(R.string.require_manager_version), error = true)
        if (state.showLkmUpdate) Notice(stringResource(R.string.home_lkm_update_available), error = false, onClick = actions.onInstallClick)
        if (state.showRootWarning) Notice(stringResource(R.string.grant_root_failed), error = true)
    }
}

@Composable
private fun Stats() {
    val pager = LocalMainPagerState.current
    val superUser by viewModel<SuperUserViewModel>().uiState.collectAsStateWithLifecycle()
    val module by viewModel<ModuleViewModel>().uiState.collectAsStateWithLifecycle()
    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        StatTile(stringResource(R.string.superuser), "shield-checkmark", superUser.groupedApps.count { it.anyAllowSu }, Modifier.weight(1f)) {
            pager.animateToPage(1)
        }
        StatTile(stringResource(R.string.module), "extension-puzzle", module.modules.size, Modifier.weight(1f)) {
            pager.animateToPage(2)
        }
    }
}

@Composable
private fun StatTile(title: String, icon: String, count: Int, modifier: Modifier, onClick: suspend () -> Unit) {
    val scheme = MochiTheme.colorScheme
    val scope = rememberCoroutineScope()
    Column(
        modifier
            .mochiPressDeform()
            .clip(HeroShape)
            .background(MochiDefaults.groupedContainer())
            .clickable(remember { MutableInteractionSource() }, indication = null) { scope.launch { onClick() } }
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            MochiIonicon(icon, tint = scheme.onSurfaceVariant, size = 16.dp)
            Text(title, style = MochiTheme.textStyles.subhead, color = scheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        MochiCounterText(
            count.toLong(),
            style = MochiTheme.textStyles.body.copy(fontSize = 28.sp, lineHeight = 34.sp, fontWeight = FontWeight.SemiBold),
            color = scheme.onSurface,
        )
    }
}

@Composable
private fun SystemInfoSection(state: HomeUiState) {
    val info = state.systemInfo
    val selinux = when (info.selinuxStatus) {
        "Enforcing" -> stringResource(R.string.selinux_status_enforcing)
        "Permissive" -> stringResource(R.string.selinux_status_permissive)
        "Disabled" -> stringResource(R.string.selinux_status_disabled)
        else -> stringResource(R.string.selinux_status_unknown)
    }
    val seccomp = when (info.seccompStatus) {
        -1 -> stringResource(R.string.seccomp_status_not_supported)
        0 -> stringResource(R.string.seccomp_status_disabled)
        1 -> stringResource(R.string.seccomp_status_strict)
        2 -> stringResource(R.string.seccomp_status_filter)
        else -> stringResource(R.string.seccomp_status_unknown)
    }
    Column {
        MochiListSectionHeader(stringResource(R.string.mochi_home_details))
        val line = MochiTheme.colorScheme.onSurface.copy(alpha = 0.10f)
        Column(
            Modifier
                .fillMaxWidth()
                .clip(HeroShape)
                .background(MochiDefaults.groupedContainer())
                .padding(vertical = 6.dp),
        ) {
            InfoPair(
                InfoCell("phone-portrait", stringResource(R.string.home_device_model), info.deviceModel),
                InfoCell("apps", stringResource(R.string.home_manager_version), info.managerVersion),
                line,
            )
            InfoDivider(line)
            InfoPair(
                InfoCell("shield-half", stringResource(R.string.home_selinux_status), selinux),
                InfoCell("lock-closed", stringResource(R.string.home_seccomp_status), seccomp),
                line,
            )
            InfoDivider(line)
            // Only the base version; the hash and build suffix would wrap badly.
            InfoCellView(InfoCell("hardware-chip", stringResource(R.string.home_kernel), shortKernelVersion(info.kernelVersion)))
            InfoDivider(line)
            InfoCellView(InfoCell("finger-print", stringResource(R.string.home_fingerprint), info.fingerprint))
        }
    }
}

private class InfoCell(val icon: String, val label: String, val value: String)

@Composable
private fun InfoPair(left: InfoCell, right: InfoCell, line: Color) {
    Row(Modifier.height(IntrinsicSize.Min)) {
        InfoCellView(left, Modifier.weight(1f))
        Box(Modifier.padding(vertical = 10.dp).width(0.5.dp).fillMaxHeight().background(line))
        InfoCellView(right, Modifier.weight(1f))
    }
}

@Composable
private fun InfoDivider(line: Color) {
    Box(Modifier.padding(horizontal = 16.dp).fillMaxWidth().height(0.5.dp).background(line))
}

@Composable
private fun InfoCellView(cell: InfoCell, modifier: Modifier = Modifier) {
    val scheme = MochiTheme.colorScheme
    Column(modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 12.dp), verticalArrangement = Arrangement.spacedBy(3.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            MochiIonicon(cell.icon, tint = scheme.onSurfaceVariant, size = 14.dp)
            Text(cell.label, style = MochiTheme.textStyles.footnote, color = scheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        Text(cell.value, style = MochiTheme.textStyles.body, color = scheme.onSurface)
    }
}

@Composable
private fun SupportSection(actions: HomeActions) {
    val learnUrl = stringResource(R.string.home_learn_kernelsu_url)
    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        SupportTile(
            "heart",
            stringResource(R.string.home_support_title),
            stringResource(R.string.mochi_home_support_short),
            Modifier.weight(1f),
        ) { actions.onOpenUrl("https://patreon.com/weishu") }
        SupportTile(
            "book",
            stringResource(R.string.home_learn_kernelsu),
            stringResource(R.string.mochi_home_learn_short),
            Modifier.weight(1f),
        ) { actions.onOpenUrl(learnUrl) }
    }
}

@Composable
private fun SupportTile(icon: String, title: String, summary: String, modifier: Modifier, onClick: () -> Unit) {
    val scheme = MochiTheme.colorScheme
    Column(
        modifier
            .mochiPressDeform()
            .clip(HeroShape)
            .background(MochiDefaults.groupedContainer())
            .clickable(remember { MutableInteractionSource() }, indication = null, onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        MochiIonicon(icon, tint = scheme.onSurfaceVariant, size = 20.dp)
        Text(
            title,
            Modifier.padding(top = 4.dp),
            style = MochiTheme.textStyles.body.copy(fontWeight = FontWeight.SemiBold),
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(summary, style = MochiTheme.textStyles.footnote, color = scheme.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
    }
}

/** 6.1.118-android14-11-g3a9c1d2e-ab123 -> 6.1.118-android14-11, unknown formats are kept as is. */
private fun shortKernelVersion(full: String): String =
    Regex("""^\d+\.\d+(\.\d+)?(-android\d+-\d+)?""").find(full)?.value?.takeIf { it.isNotEmpty() } ?: full

private val GroupGap = 16.dp

/** Adds [gap] above the content, or nothing when the content has zero height. */
private fun Modifier.gapAboveIfNotEmpty(gap: Dp): Modifier = layout { measurable, constraints ->
    val p = measurable.measure(constraints)
    if (p.height == 0) {
        layout(p.width, 0) {}
    } else {
        val g = gap.roundToPx()
        layout(p.width, p.height + g) { p.place(0, g) }
    }
}
