package me.weishu.kernelsu.ui.screen.home

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CheckCircleOutline
import androidx.compose.material.icons.rounded.ErrorOutline
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.KernelVersion
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.getKernelVersion
import me.weishu.kernelsu.magica.MagicaService
import me.weishu.kernelsu.ui.LocalMainPagerState
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.component.rebootlistpopup.RebootListPopupMiuix
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.isInDarkTheme
import me.weishu.kernelsu.ui.util.checkNewVersion
import me.weishu.kernelsu.ui.util.getModuleCount
import me.weishu.kernelsu.ui.util.getSuperuserCount
import me.weishu.kernelsu.ui.util.isSELinuxPermissive
import me.weishu.kernelsu.ui.util.module.LatestVersionInfo
import me.weishu.kernelsu.ui.util.rootAvailable
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Link
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.theme.MiuixTheme.isDynamicColor
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun HomePagerMiuix(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    val kernelVersion = getKernelVersion()
    val scrollBehavior = MiuixScrollBehavior()
    val enableBlur = LocalEnableBlur.current
    val hazeState = remember { HazeState() }
    val hazeStyle = if (enableBlur) {
        HazeStyle(
            backgroundColor = colorScheme.surface,
            tint = HazeTint(colorScheme.surface.copy(0.8f))
        )
    } else {
        HazeStyle.Unspecified
    }

    val context = LocalContext.current
    val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
    val checkUpdate = prefs.getBoolean("check_update", true)

    Scaffold(
        topBar = {
            TopBar(
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
                .padding(horizontal = 12.dp)
                .let { if (enableBlur) it.hazeSource(state = hazeState) else it },
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                val loadingDialog = rememberLoadingDialog()
                var refreshKey by remember { mutableIntStateOf(0) }

                val isManager = remember(refreshKey) { Natives.isManager }
                val ksuVersion = remember(refreshKey) { if (isManager) Natives.version else null }
                val lkmMode = remember(refreshKey) {
                    ksuVersion?.let {
                        if (kernelVersion.isGKI()) Natives.isLkmMode else null
                    }
                }
                val mainState = LocalMainPagerState.current

                Column(
                    modifier = Modifier.padding(vertical = 12.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    if (isManager && BuildConfig.IS_PR_BUILD) {
                        WarningCard(stringResource(id = R.string.home_pr_build_warning))
                    }
                    if (isManager && !BuildConfig.IS_PR_BUILD && Natives.isPrBuild) {
                        WarningCard(stringResource(id = R.string.home_pr_kernel_warning))
                    }
                    if (ksuVersion != null && !Natives.isLkmMode) {
                        WarningCard(stringResource(id = R.string.home_gki_warning))
                    }
                    if (isManager && Natives.requireNewKernel()) {
                        WarningCard(
                            stringResource(id = R.string.require_kernel_version)
                                .format(ksuVersion, Natives.MINIMAL_SUPPORTED_KERNEL),
                        )
                    }
                    if (ksuVersion != null && !rootAvailable()) {
                        WarningCard(stringResource(id = R.string.grant_root_failed))
                    }
                    StatusCard(
                        kernelVersion, ksuVersion, lkmMode,
                        isSafeMode = remember(refreshKey) { Natives.isSafeMode },
                        isLateLoadMode = remember(refreshKey) { Natives.isLateLoadMode },
                        isSELinuxPermissive = isSELinuxPermissive(),
                        superuserCount = getSuperuserCount(),
                        moduleCount = getModuleCount(),
                        onClickInstall = {
                            navigator.push(Route.Install)
                        },
                        onClickJailbreak = {
                            loadingDialog.showLoading()
                            val intent = Intent(context, MagicaService::class.java)
                            context.bindService(intent, object : ServiceConnection {
                                override fun onServiceConnected(name: ComponentName?, service: IBinder?) {}
                                override fun onServiceDisconnected(name: ComponentName?) {
                                    context.unbindService(this)
                                    loadingDialog.hide()
                                    refreshKey++
                                }
                            }, Context.BIND_AUTO_CREATE)
                        },
                        onClickSuperuser = {
                            mainState.animateToPage(1)
                        },
                        onclickModule = {
                            mainState.animateToPage(2)
                        },
                    )

                    if (checkUpdate) {
                        UpdateCard()
                    }
                    InfoCard()
                    DonateCard()
                    LearnMoreCard()
                }
                Spacer(Modifier.height(bottomInnerPadding))
            }
        }
    }
}

@Composable
private fun UpdateCard() {
    val context = LocalContext.current
    val latestVersionInfo = LatestVersionInfo()
    val newVersion by produceState(initialValue = latestVersionInfo) {
        value = withContext(Dispatchers.IO) {
            checkNewVersion()
        }
    }

    val currentVersionCode = getManagerVersion(context).versionCode
    val newVersionCode = newVersion.versionCode
    val newVersionUrl = newVersion.downloadUrl
    val changelog = newVersion.changelog

    val uriHandler = LocalUriHandler.current
    val title = stringResource(id = R.string.module_changelog)
    val updateText = stringResource(id = R.string.module_update)

    AnimatedVisibility(
        visible = newVersionCode > currentVersionCode,
        enter = fadeIn() + expandVertically(),
        exit = shrinkVertically() + fadeOut()
    ) {
        val updateDialog = rememberConfirmDialog(onConfirm = { uriHandler.openUri(newVersionUrl) })
        WarningCard(
            message = stringResource(id = R.string.new_version_available).format(newVersionCode),
            colorScheme.outline
        ) {
            if (changelog.isEmpty()) {
                uriHandler.openUri(newVersionUrl)
            } else {
                updateDialog.showConfirm(
                    title = title,
                    content = changelog,
                    markdown = true,
                    confirm = updateText
                )
            }
        }
    }
}

@Composable
private fun TopBar(
    scrollBehavior: ScrollBehavior,
    hazeState: HazeState,
    hazeStyle: HazeStyle,
    enableBlur: Boolean,
) {
    TopAppBar(
        modifier = if (enableBlur) {
            Modifier.hazeEffect(hazeState) {
                style = hazeStyle
                blurRadius = 30.dp
                noiseFactor = 0f
            }
        } else {
            Modifier
        },
        color = if (enableBlur) Color.Transparent else colorScheme.surface,
        title = stringResource(R.string.app_name),
        actions = {
            RebootListPopupMiuix(
                modifier = Modifier.padding(end = 16.dp),
            )
        },
        scrollBehavior = scrollBehavior
    )
}

@Composable
private fun StatusCard(
    kernelVersion: KernelVersion,
    ksuVersion: Int?,
    lkmMode: Boolean?,
    isSafeMode: Boolean,
    isLateLoadMode: Boolean,
    isSELinuxPermissive: Boolean,
    superuserCount: Int,
    moduleCount: Int,
    onClickInstall: () -> Unit = {},
    onClickJailbreak: () -> Unit = {},
    onClickSuperuser: () -> Unit = {},
    onclickModule: () -> Unit = {},
) {
    Column(
        modifier = Modifier
    ) {
        when {
            ksuVersion != null -> {
                val workingState = buildString {
                    if (isSafeMode) {
                        append(" [${stringResource(id = R.string.safe_mode)}]")
                    }
                    if (isLateLoadMode) {
                        append(" [${stringResource(id = R.string.jailbreak_mode)}]")
                    }
                }

                val workingMode = when (lkmMode) {
                    null -> ""
                    true -> " <LKM>"
                    else -> " <GKI>"
                }

                val workingText = "${stringResource(id = R.string.home_working)}$workingMode$workingState"

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(IntrinsicSize.Min),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Card(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight(),
                        colors = CardDefaults.defaultColors(
                            color = when {
                                isDynamicColor -> colorScheme.secondaryContainer
                                isInDarkTheme() -> Color(0xFF1A3825)
                                else -> Color(0xFFDFFAE4)
                            }
                        ),
                        onClick = {
                            onClickInstall()
                        },
                        showIndication = true,
                        pressFeedbackType = PressFeedbackType.Tilt
                    ) {
                        Box(
                            modifier = Modifier.fillMaxSize()
                        ) {
                            Box(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .offset(38.dp, 45.dp),
                                contentAlignment = Alignment.BottomEnd
                            ) {
                                Icon(
                                    modifier = Modifier.size(170.dp),
                                    imageVector = Icons.Rounded.CheckCircleOutline,
                                    tint = if (isDynamicColor) {
                                        colorScheme.primary.copy(alpha = 0.8f)
                                    } else {
                                        Color(0xFF36D167)
                                    },
                                    contentDescription = null
                                )
                            }
                            Column(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .padding(all = 16.dp)
                            ) {
                                Text(
                                    modifier = Modifier.fillMaxWidth(),
                                    text = workingText,
                                    fontSize = 20.sp,
                                    fontWeight = FontWeight.SemiBold,
                                )
                                Spacer(Modifier.height(2.dp))
                                Text(
                                    modifier = Modifier.fillMaxWidth(),
                                    text = stringResource(R.string.home_working_version, ksuVersion),
                                    fontSize = 14.sp,
                                    fontWeight = FontWeight.Medium,
                                )
                            }
                        }
                    }
                    Column(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight()
                    ) {
                        Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f),
                            insideMargin = PaddingValues(16.dp),
                            onClick = { onClickSuperuser() },
                            showIndication = true,
                            pressFeedbackType = PressFeedbackType.Tilt
                        ) {
                            Column(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalAlignment = Alignment.Start
                            ) {
                                Text(
                                    modifier = Modifier.fillMaxWidth(),
                                    text = stringResource(R.string.superuser),
                                    fontWeight = FontWeight.Medium,
                                    fontSize = 15.sp,
                                    color = colorScheme.onSurfaceVariantSummary,
                                )
                                Text(
                                    modifier = Modifier.fillMaxWidth(),
                                    text = superuserCount.toString(),
                                    fontSize = 26.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = colorScheme.onSurface,
                                )
                            }
                        }
                        Spacer(Modifier.height(12.dp))
                        Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f),
                            insideMargin = PaddingValues(16.dp),
                            onClick = { onclickModule() },
                            showIndication = true,
                            pressFeedbackType = PressFeedbackType.Tilt
                        ) {
                            Column(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalAlignment = Alignment.Start
                            ) {
                                Text(
                                    modifier = Modifier.fillMaxWidth(),
                                    text = stringResource(R.string.module),
                                    fontWeight = FontWeight.Medium,
                                    fontSize = 15.sp,
                                    color = colorScheme.onSurfaceVariantSummary,
                                )
                                Text(
                                    modifier = Modifier.fillMaxWidth(),
                                    text = moduleCount.toString(),
                                    fontSize = 26.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = colorScheme.onSurface,
                                )
                            }
                        }
                    }
                }
            }

            kernelVersion.isGKI() -> {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Card(
                        modifier = Modifier.weight(1f),
                        onClick = {
                            onClickInstall()
                        },
                        showIndication = true,
                        pressFeedbackType = PressFeedbackType.Sink
                    ) {
                        BasicComponent(
                            title = stringResource(R.string.home_not_installed),
                            summary = stringResource(R.string.home_click_to_install),
                            startAction = {
                                Icon(
                                    Icons.Rounded.ErrorOutline,
                                    stringResource(R.string.home_not_installed),
                                    modifier = Modifier
                                        .padding(end = 16.dp),
                                    tint = colorScheme.onBackground,
                                )
                            },
                            endActions = {
                                if (isSELinuxPermissive) {
                                    TextButton(
                                        text = stringResource(R.string.home_jailbreak),
                                        insideMargin = PaddingValues(12.dp),
                                        onClick = {
                                            onClickJailbreak()
                                        },
                                        colors = ButtonDefaults.textButtonColorsPrimary()
                                    )
                                }
                            }
                        )
                    }
                }
            }

            else -> {
                Card(
                    onClick = {
                        onClickInstall()
                    },
                    showIndication = true,
                    pressFeedbackType = PressFeedbackType.Sink
                ) {
                    BasicComponent(
                        title = stringResource(R.string.home_unsupported),
                        summary = stringResource(R.string.home_unsupported_reason),
                        startAction = {
                            Icon(
                                Icons.Rounded.ErrorOutline,
                                stringResource(R.string.home_unsupported),
                                modifier = Modifier
                                    .padding(end = 16.dp),
                                tint = colorScheme.onBackground,
                            )
                        }
                    )
                }
            }
        }
    }
}

@Composable
private fun WarningCard(
    message: String,
    color: Color? = null,
    onClick: (() -> Unit)? = null,
) {
    Card(
        onClick = {
            onClick?.invoke()
        },
        colors = CardDefaults.defaultColors(
            color = color ?: when {
                isDynamicColor -> colorScheme.errorContainer
                isInDarkTheme() -> Color(0XFF310808)
                else -> Color(0xFFF8E2E2)
            }
        ),
        showIndication = onClick != null,
        pressFeedbackType = PressFeedbackType.Tilt
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            Text(
                text = message,
                color = if (isDynamicColor) colorScheme.onErrorContainer else Color(0xFFF72727),
                fontSize = 14.sp
            )
        }
    }
}

@Composable
private fun LearnMoreCard() {
    val uriHandler = LocalUriHandler.current
    val url = stringResource(R.string.home_learn_kernelsu_url)

    Card(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        BasicComponent(
            title = stringResource(R.string.home_learn_kernelsu),
            summary = stringResource(R.string.home_click_to_learn_kernelsu),
            endActions = {
                Icon(
                    imageVector = MiuixIcons.Link,
                    tint = colorScheme.onSurface,
                    contentDescription = null
                )
            },
            onClick = {
                uriHandler.openUri(url)
            }
        )
    }
}

@Composable
private fun DonateCard() {
    val uriHandler = LocalUriHandler.current

    Card(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        BasicComponent(
            title = stringResource(R.string.home_support_title),
            summary = stringResource(R.string.home_support_content),
            endActions = {
                Icon(
                    imageVector = MiuixIcons.Link,
                    tint = colorScheme.onSurface,
                    contentDescription = null
                )
            },
            onClick = {
                uriHandler.openUri("https://patreon.com/weishu")
            },
            insideMargin = PaddingValues(18.dp)
        )
    }
}

@Composable
private fun InfoCard(systemInfo: SystemInfo = rememberSystemInfo()) {
    @Composable
    fun InfoText(
        title: String,
        content: String,
        bottomPadding: Dp = 24.dp
    ) {
        Text(
            text = title,
            fontSize = MiuixTheme.textStyles.headline1.fontSize,
            fontWeight = FontWeight.Medium,
            color = colorScheme.onSurface
        )
        Text(
            text = content,
            fontSize = MiuixTheme.textStyles.body2.fontSize,
            color = colorScheme.onSurfaceVariantSummary,
            modifier = Modifier.padding(top = 2.dp, bottom = bottomPadding)
        )
    }
    Card {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            InfoText(
                title = stringResource(R.string.home_kernel),
                content = systemInfo.kernelVersion
            )
            InfoText(
                title = stringResource(R.string.home_manager_version),
                content = systemInfo.managerVersion
            )
            InfoText(
                title = stringResource(R.string.home_fingerprint),
                content = systemInfo.fingerprint
            )
            InfoText(
                title = stringResource(R.string.home_selinux_status),
                content = systemInfo.selinuxStatus,
                bottomPadding = 0.dp
            )
        }
    }
}

@Preview(name = "Activated")
@Composable
private fun StatusCardActivatedPreview() {
    StatusCard(
        kernelVersion = KernelVersion(6, 1, 0),
        ksuVersion = 12345,
        lkmMode = true,
        isSafeMode = false,
        isLateLoadMode = false,
        isSELinuxPermissive = false,
        superuserCount = 5,
        moduleCount = 10,
    )
}

@Preview(name = "Not Activated")
@Composable
private fun StatusCardNotActivatedPreview() {
    StatusCard(
        kernelVersion = KernelVersion(6, 1, 0),
        ksuVersion = null,
        lkmMode = null,
        isSafeMode = false,
        isLateLoadMode = false,
        isSELinuxPermissive = false,
        superuserCount = 0,
        moduleCount = 0,
    )
}

@Preview(name = "Permissive")
@Composable
private fun StatusCardPermissivePreview() {
    StatusCard(
        kernelVersion = KernelVersion(6, 1, 0),
        ksuVersion = null,
        lkmMode = null,
        isSafeMode = false,
        isLateLoadMode = false,
        isSELinuxPermissive = true,
        superuserCount = 0,
        moduleCount = 0,
    )
}

@Preview(name = "Jailbreak")
@Composable
private fun StatusCardJailbreakPreview() {
    StatusCard(
        kernelVersion = KernelVersion(6, 1, 0),
        ksuVersion = 12345,
        lkmMode = true,
        isSafeMode = false,
        isLateLoadMode = true,
        isSELinuxPermissive = false,
        superuserCount = 5,
        moduleCount = 10,
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
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 12.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            StatusCard(
                kernelVersion = KernelVersion(6, 1, 0),
                ksuVersion = ksuVersion,
                lkmMode = lkmMode,
                isSafeMode = isSafeMode,
                isLateLoadMode = isLateLoadMode,
                isSELinuxPermissive = isSELinuxPermissive,
                superuserCount = superuserCount,
                moduleCount = moduleCount,
            )
            InfoCard(previewSystemInfo.copy(selinuxStatus = selinuxStatus))
            DonateCard()
            LearnMoreCard()
        }
    }
}

@Preview(name = "Home Activated", showBackground = true)
@Composable
private fun HomeScreenActivatedPreview() {
    HomeScreenPreviewContent(
        ksuVersion = 12345,
        lkmMode = true,
        superuserCount = 5,
        moduleCount = 10,
    )
}

@Preview(name = "Home Not Activated", showBackground = true)
@Composable
private fun HomeScreenNotActivatedPreview() {
    HomeScreenPreviewContent(
        ksuVersion = null,
        lkmMode = null,
    )
}

@Preview(name = "Home Permissive", showBackground = true)
@Composable
private fun HomeScreenPermissivePreview() {
    HomeScreenPreviewContent(
        ksuVersion = null,
        lkmMode = null,
        isSELinuxPermissive = true,
        selinuxStatus = "Permissive",
    )
}

@Preview(name = "Home Jailbreak", showBackground = true)
@Composable
private fun HomeScreenJailbreakPreview() {
    HomeScreenPreviewContent(
        ksuVersion = 12345,
        lkmMode = true,
        isLateLoadMode = true,
        superuserCount = 5,
        moduleCount = 10,
    )
}
