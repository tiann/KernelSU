package me.weishu.kernelsu.ui.screen.home

import android.content.Intent
import android.widget.Toast
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Block
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Warning
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.material3.surfaceColorAtElevation
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.KernelVersion
import me.weishu.kernelsu.R
import me.weishu.kernelsu.magica.MagicaService
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.component.rebootlistpopup.RebootListPopup
import me.weishu.kernelsu.ui.component.statustag.StatusTag

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomePagerMaterial(
    state: HomeUiState,
    actions: HomeActions,
    bottomInnerPadding: Dp,
) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val context = LocalContext.current
    val loadingDialog = rememberLoadingDialog()
    val scope = rememberCoroutineScope()

    Scaffold(
        topBar = { TopBar(scrollBehavior = scrollBehavior) },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            StatusCard(
                state = state,
                actions = actions,
                onClickJailbreak = {
                    loadingDialog.showLoading()
                    context.startService(Intent(context, MagicaService::class.java))
                    scope.launch(Dispatchers.IO) {
                        delay(30_000)
                        withContext(Dispatchers.Main) {
                            loadingDialog.hide()
                            Toast.makeText(context, R.string.jailbreak_timeout, Toast.LENGTH_LONG).show()
                        }
                    }
                },
            )
            if (state.showManagerPrBuildWarning) {
                WarningCard(stringResource(id = R.string.home_pr_build_warning))
            } else if (state.showKernelPrBuildWarning) {
                WarningCard(stringResource(id = R.string.home_pr_kernel_warning))
            }
            if (state.showGkiWarning) {
                WarningCard(
                    stringResource(id = R.string.home_gki_warning),
                    MaterialTheme.colorScheme.tertiaryContainer
                )
            }
            if (state.showRequireKernelWarning) {
                WarningCard(
                    stringResource(id = R.string.require_kernel_version).format(
                        state.ksuVersion,
                        me.weishu.kernelsu.Natives.MINIMAL_SUPPORTED_KERNEL
                    )
                )
            }
            if (state.showRootWarning) {
                WarningCard(stringResource(id = R.string.grant_root_failed))
            }
            if (state.checkUpdateEnabled) {
                UpdateCard(state = state, actions = actions)
            }
            InfoCard(systemInfo = state.systemInfo)
            DonateCard(onOpenUrl = actions.onOpenUrl)
            LearnMoreCard(onOpenUrl = actions.onOpenUrl)
            Spacer(Modifier.height(bottomInnerPadding))
        }
    }
}

@Composable
private fun UpdateCard(
    state: HomeUiState,
    actions: HomeActions,
) {
    val newVersion = state.latestVersionInfo
    val title = stringResource(id = R.string.module_changelog)
    val updateText = stringResource(id = R.string.module_update)

    AnimatedVisibility(
        visible = state.hasUpdate,
        enter = fadeIn() + expandVertically(),
        exit = shrinkVertically() + fadeOut()
    ) {
        val updateDialog = rememberConfirmDialog(onConfirm = { actions.onOpenUrl(newVersion.downloadUrl) })
        WarningCard(
            message = stringResource(id = R.string.new_version_available).format(newVersion.versionCode),
            MaterialTheme.colorScheme.outlineVariant
        ) {
            if (newVersion.changelog.isEmpty()) {
                actions.onOpenUrl(newVersion.downloadUrl)
            } else {
                updateDialog.showConfirm(
                    title = title,
                    content = newVersion.changelog,
                    markdown = true,
                    confirm = updateText
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun TopBar(
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    LargeFlexibleTopAppBar(
        title = { Text(stringResource(R.string.app_name)) },
        actions = { RebootListPopup() },
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface,
            scrolledContainerColor = MaterialTheme.colorScheme.surface
        ),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}

@Composable
private fun StatusCard(
    state: HomeUiState,
    actions: HomeActions,
    onClickJailbreak: () -> Unit = {},
) {
    Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
        TonalCard(
            containerColor = if (state.ksuVersion != null) {
                MaterialTheme.colorScheme.secondaryContainer
            } else {
                MaterialTheme.colorScheme.errorContainer
            }
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { actions.onInstallClick() }
                    .padding(24.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                when {
                    state.ksuVersion != null -> {
                        val workingMode = when (state.lkmMode) {
                            null -> ""
                            true -> "LKM"
                            else -> "GKI"
                        }

                        Icon(Icons.Outlined.CheckCircle, stringResource(R.string.home_working))
                        Column(Modifier.padding(start = 20.dp)) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                    text = stringResource(id = R.string.home_working),
                                    style = MaterialTheme.typography.titleMedium
                                )
                                if (workingMode.isNotEmpty()) {
                                    Spacer(Modifier.width(8.dp))
                                    StatusTag(
                                        label = workingMode,
                                        contentColor = MaterialTheme.colorScheme.onPrimary,
                                        backgroundColor = MaterialTheme.colorScheme.primary
                                    )
                                }
                                if (state.isSafeMode) {
                                    Spacer(Modifier.width(8.dp))
                                    StatusTag(
                                        label = stringResource(id = R.string.safe_mode),
                                        contentColor = MaterialTheme.colorScheme.onErrorContainer,
                                        backgroundColor = MaterialTheme.colorScheme.errorContainer
                                    )
                                }
                                if (state.isLateLoadMode) {
                                    Spacer(Modifier.width(8.dp))
                                    StatusTag(
                                        label = stringResource(id = R.string.jailbreak_mode),
                                        contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                                        backgroundColor = MaterialTheme.colorScheme.tertiaryContainer
                                    )
                                }
                            }
                            Spacer(Modifier.height(4.dp))
                            Text(
                                text = stringResource(R.string.home_working_version, state.ksuVersion),
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                    }

                    state.kernelVersion.isGKI() -> {
                        Icon(Icons.Outlined.Warning, stringResource(R.string.home_not_installed))
                        Column(
                            modifier = Modifier
                                .padding(start = 20.dp)
                                .weight(1f)
                        ) {
                            Text(
                                text = stringResource(R.string.home_not_installed),
                                style = MaterialTheme.typography.titleMedium
                            )
                            Spacer(Modifier.height(4.dp))
                            Text(
                                text = stringResource(R.string.home_click_to_install),
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                        if (state.isSELinuxPermissive) {
                            Button(
                                onClick = onClickJailbreak,
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = MaterialTheme.colorScheme.error,
                                    contentColor = MaterialTheme.colorScheme.onError
                                )
                            ) {
                                Text(stringResource(R.string.home_jailbreak))
                            }
                        }
                    }

                    else -> {
                        Icon(Icons.Outlined.Block, stringResource(R.string.home_unsupported))
                        Column(Modifier.padding(start = 20.dp)) {
                            Text(
                                text = stringResource(R.string.home_unsupported),
                                style = MaterialTheme.typography.titleMedium
                            )
                            Spacer(Modifier.height(4.dp))
                            Text(
                                text = stringResource(R.string.home_unsupported_reason),
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                    }
                }
            }
        }
        if (state.isFullFeatured) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                TonalCard(modifier = Modifier.weight(1f)) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { actions.onSuperuserClick() }
                            .padding(horizontal = 24.dp, vertical = 16.dp)
                    ) {
                        Text(text = stringResource(R.string.superuser), style = MaterialTheme.typography.bodyLarge)
                        Spacer(Modifier.height(4.dp))
                        Text(
                            text = state.superuserCount.toString(),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.outline
                        )
                    }
                }
                TonalCard(modifier = Modifier.weight(1f)) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { actions.onModuleClick() }
                            .padding(horizontal = 24.dp, vertical = 16.dp)
                    ) {
                        Text(text = stringResource(R.string.module), style = MaterialTheme.typography.bodyLarge)
                        Spacer(Modifier.height(4.dp))
                        Text(
                            text = state.moduleCount.toString(),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.outline
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun WarningCard(
    message: String,
    color: Color = MaterialTheme.colorScheme.error,
    onClick: (() -> Unit)? = null
) {
    TonalCard(containerColor = color) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .then(onClick?.let { Modifier.clickable { it() } } ?: Modifier)
                .padding(24.dp)
        ) {
            Text(text = message, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
fun TonalCard(
    modifier: Modifier = Modifier,
    containerColor: Color = MaterialTheme.colorScheme.surfaceColorAtElevation(1.dp),
    shape: Shape = MaterialTheme.shapes.large,
    content: @Composable () -> Unit
) {
    Card(
        modifier = modifier,
        colors = CardDefaults.cardColors(containerColor = containerColor),
        shape = shape
    ) {
        content()
    }
}

@Composable
private fun LearnMoreCard(onOpenUrl: (String) -> Unit) {
    val url = stringResource(R.string.home_learn_kernelsu_url)
    TonalCard {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onOpenUrl(url) }
                .padding(24.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(text = stringResource(R.string.home_learn_kernelsu), style = MaterialTheme.typography.titleSmall)
                Spacer(Modifier.height(4.dp))
                Text(
                    text = stringResource(R.string.home_click_to_learn_kernelsu),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.outline
                )
            }
        }
    }
}

@Composable
private fun DonateCard(onOpenUrl: (String) -> Unit) {
    TonalCard {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onOpenUrl("https://patreon.com/weishu") }
                .padding(24.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(text = stringResource(R.string.home_support_title), style = MaterialTheme.typography.titleSmall)
                Spacer(Modifier.height(4.dp))
                Text(
                    text = stringResource(R.string.home_support_content),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.outline
                )
            }
        }
    }
}

@Composable
private fun InfoCard(systemInfo: SystemInfo) {
    TonalCard {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 24.dp, top = 24.dp, end = 24.dp, bottom = 16.dp)
        ) {
            @Composable
            fun InfoCardItem(label: String, content: String) {
                Text(text = label, style = MaterialTheme.typography.bodyLarge)
                Text(
                    text = content,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.outline
                )
            }

            InfoCardItem(stringResource(R.string.home_kernel), systemInfo.kernelVersion)
            Spacer(Modifier.height(16.dp))
            InfoCardItem(stringResource(R.string.home_manager_version), systemInfo.managerVersion)
            Spacer(Modifier.height(16.dp))
            InfoCardItem(stringResource(R.string.home_fingerprint), systemInfo.fingerprint)
            Spacer(Modifier.height(16.dp))
            InfoCardItem(stringResource(R.string.home_selinux_status), systemInfo.selinuxStatus)
        }
    }
}

@Preview(name = "Activated")
@Composable
private fun StatusCardActivatedPreview() {
    StatusCard(
        state = previewHomeScreenState(ksuVersion = 12345, lkmMode = true, superuserCount = 5, moduleCount = 10),
        actions = HomeActions({}, {}, {}, {})
    )
}

@Preview(name = "Not Activated")
@Composable
private fun StatusCardNotActivatedPreview() {
    StatusCard(state = previewHomeScreenState(ksuVersion = null, lkmMode = null), actions = HomeActions({}, {}, {}, {}))
}

@Preview(name = "Permissive")
@Composable
private fun StatusCardPermissivePreview() {
    StatusCard(
        state = previewHomeScreenState(ksuVersion = null, lkmMode = null, isSELinuxPermissive = true),
        actions = HomeActions({}, {}, {}, {})
    )
}

@Preview(name = "Jailbreak")
@Composable
private fun StatusCardJailbreakPreview() {
    StatusCard(
        state = previewHomeScreenState(ksuVersion = 12345, lkmMode = true, isLateLoadMode = true, superuserCount = 5, moduleCount = 10),
        actions = HomeActions({}, {}, {}, {})
    )
}

private val previewSystemInfo = SystemInfo(
    kernelVersion = "6.1.0-android14-0-g1234567",
    managerVersion = "1.0.0 (10000)",
    fingerprint = "google/raven/raven:14/AP1A.240305.019:user/release-keys",
    selinuxStatus = "Enforcing"
)

private val previewUriHandler = object : UriHandler {
    override fun openUri(uri: String) {}
}

@Composable
private fun HomeScreenPreviewContent(
    ksuVersion: Int?,
    lkmMode: Boolean?,
    isSafeMode: Boolean = false,
    isLateLoadMode: Boolean = false,
    isSELinuxPermissive: Boolean = false,
    superuserCount: Int = 0,
    moduleCount: Int = 0,
    selinuxStatus: String = "Enforcing",
) {
    CompositionLocalProvider(LocalUriHandler provides previewUriHandler) {
        Column(
            modifier = Modifier.padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            val actions = HomeActions({}, {}, {}, {})
            StatusCard(
                state = previewHomeScreenState(
                    ksuVersion = ksuVersion,
                    lkmMode = lkmMode,
                    isSafeMode = isSafeMode,
                    isLateLoadMode = isLateLoadMode,
                    isSELinuxPermissive = isSELinuxPermissive,
                    superuserCount = superuserCount,
                    moduleCount = moduleCount,
                    selinuxStatus = selinuxStatus,
                ),
                actions = actions
            )
            InfoCard(previewSystemInfo.copy(selinuxStatus = selinuxStatus))
            DonateCard(onOpenUrl = {})
            LearnMoreCard(onOpenUrl = {})
        }
    }
}

@Preview(name = "Home Activated", showBackground = true)
@Composable
private fun HomeScreenActivatedPreview() {
    HomeScreenPreviewContent(ksuVersion = 12345, lkmMode = true, superuserCount = 5, moduleCount = 10)
}

@Preview(name = "Home Not Activated", showBackground = true)
@Composable
private fun HomeScreenNotActivatedPreview() {
    HomeScreenPreviewContent(ksuVersion = null, lkmMode = null)
}

@Preview(name = "Home Permissive", showBackground = true)
@Composable
private fun HomeScreenPermissivePreview() {
    HomeScreenPreviewContent(ksuVersion = null, lkmMode = null, isSELinuxPermissive = true, selinuxStatus = "Permissive")
}

@Preview(name = "Home Jailbreak", showBackground = true)
@Composable
private fun HomeScreenJailbreakPreview() {
    HomeScreenPreviewContent(ksuVersion = 12345, lkmMode = true, isLateLoadMode = true, superuserCount = 5, moduleCount = 10)
}

private fun previewHomeScreenState(
    ksuVersion: Int?,
    lkmMode: Boolean?,
    isSafeMode: Boolean = false,
    isLateLoadMode: Boolean = false,
    isSELinuxPermissive: Boolean = false,
    superuserCount: Int = 0,
    moduleCount: Int = 0,
    selinuxStatus: String = "Enforcing",
) = HomeUiState(
    kernelVersion = KernelVersion(6, 1, 0),
    ksuVersion = ksuVersion,
    lkmMode = lkmMode,
    isManager = true,
    isManagerPrBuild = false,
    isKernelPrBuild = false,
    requiresNewKernel = false,
    isRootAvailable = ksuVersion != null,
    isSafeMode = isSafeMode,
    isLateLoadMode = isLateLoadMode,
    isSELinuxPermissive = isSELinuxPermissive,
    checkUpdateEnabled = false,
    latestVersionInfo = me.weishu.kernelsu.ui.util.module.LatestVersionInfo(),
    currentManagerVersionCode = 10000,
    superuserCount = superuserCount,
    moduleCount = moduleCount,
    systemInfo = previewSystemInfo.copy(selinuxStatus = selinuxStatus),
)
