package me.weishu.kernelsu.ui.screen

import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.system.Os
import androidx.annotation.StringRes
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.isSystemInDarkTheme
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
import androidx.compose.foundation.pager.PagerState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CheckCircleOutline
import androidx.compose.material.icons.rounded.ErrorOutline
import androidx.compose.material.icons.rounded.Link
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.pm.PackageInfoCompat
import com.ramcosta.composedestinations.generated.destinations.InstallScreenDestination
import com.ramcosta.composedestinations.generated.destinations.SettingScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.KernelVersion
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.getKernelVersion
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.DropdownItem
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.util.checkNewVersion
import me.weishu.kernelsu.ui.util.getModuleCount
import me.weishu.kernelsu.ui.util.getSELinuxStatus
import me.weishu.kernelsu.ui.util.getSuperuserCount
import me.weishu.kernelsu.ui.util.module.LatestVersionInfo
import me.weishu.kernelsu.ui.util.reboot
import me.weishu.kernelsu.ui.util.rootAvailable
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopup
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Reboot
import top.yukonga.miuix.kmp.icon.icons.useful.Save
import top.yukonga.miuix.kmp.icon.icons.useful.Settings
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun HomePager(
    pagerState: PagerState,
    navigator: DestinationsNavigator,
    bottomInnerPadding: Dp
) {
    val kernelVersion = getKernelVersion()
    val scrollBehavior = MiuixScrollBehavior()

    Scaffold(
        topBar = {
            TopBar(
                kernelVersion = kernelVersion,
                onSettingsClick = {
                    navigator.navigate(SettingScreenDestination) {
                        popUpTo(SettingScreenDestination) {
                            inclusive = true
                        }
                        launchSingleTop = true
                    }
                },
                onInstallClick = {
                    navigator.navigate(InstallScreenDestination) {
                        popUpTo(InstallScreenDestination) {
                            inclusive = true
                        }
                        launchSingleTop = true
                    }
                },
                scrollBehavior = scrollBehavior,
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .height(getWindowSize().height.dp)
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .padding(horizontal = 12.dp),
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                val coroutineScope = rememberCoroutineScope()
                val isManager = Natives.becomeManager(ksuApp.packageName)
                val ksuVersion = if (isManager) Natives.version else null
                val lkmMode = ksuVersion?.let {
                    if (it >= Natives.MINIMAL_SUPPORTED_KERNEL_LKM && kernelVersion.isGKI()) Natives.isLkmMode else null
                }

                Column(
                    modifier = Modifier.padding(vertical = 12.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    if (isManager && Natives.requireNewKernel()) {
                        WarningCard(
                            stringResource(id = R.string.require_kernel_version).format(
                                ksuVersion, Natives.MINIMAL_SUPPORTED_KERNEL
                            )
                        )
                    }
                    if (ksuVersion != null && !rootAvailable()) {
                        WarningCard(
                            stringResource(id = R.string.grant_root_failed)
                        )
                    }
                    StatusCard(
                        kernelVersion, ksuVersion, lkmMode,
                        onClickInstall = {
                            navigator.navigate(InstallScreenDestination) {
                                launchSingleTop = true
                            }
                        },
                        onClickSuperuser = {
                            coroutineScope.launch {
                                pagerState.animateScrollToPage(1)
                            }
                        },
                        onclickModule = {
                            coroutineScope.launch {
                                pagerState.animateScrollToPage(2)
                            }
                        }
                    )

                    val checkUpdate =
                        LocalContext.current.getSharedPreferences("settings", Context.MODE_PRIVATE)
                            .getBoolean("check_update", true)
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
fun UpdateCard() {
    val context = LocalContext.current
    val latestVersionInfo = LatestVersionInfo()
    val newVersion by produceState(initialValue = latestVersionInfo) {
        value = withContext(Dispatchers.IO) {
            checkNewVersion()
        }
    }

    val currentVersionCode = getManagerVersion(context).second
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
fun RebootDropdownItem(
    @StringRes id: Int, reason: String = "",
    showTopPopup: MutableState<Boolean>,
    optionSize: Int,
    index: Int,
) {
    DropdownItem(
        text = stringResource(id),
        optionSize = optionSize,
        onSelectedIndexChange = {
            reboot(reason)
            showTopPopup.value = false
        },
        index = index
    )
}

@Composable
private fun TopBar(
    kernelVersion: KernelVersion,
    onInstallClick: () -> Unit,
    onSettingsClick: () -> Unit,
    scrollBehavior: ScrollBehavior,
) {
    TopAppBar(
        title = stringResource(R.string.app_name),
        navigationIcon = {
            IconButton(
                modifier = Modifier.padding(start = 16.dp),
                onClick = onSettingsClick
            ) {
                Icon(
                    imageVector = MiuixIcons.Useful.Settings,
                    contentDescription = stringResource(id = R.string.settings),
                    tint = colorScheme.onBackground
                )
            }
        },
        actions = {
            if (kernelVersion.isGKI()) {
                IconButton(
                    modifier = Modifier.padding(end = 16.dp),
                    onClick = onInstallClick,
                ) {
                    Icon(
                        imageVector = MiuixIcons.Useful.Save,
                        contentDescription = stringResource(id = R.string.install),
                        tint = colorScheme.onBackground
                    )
                }
            }
            val showTopPopup = remember { mutableStateOf(false) }
            KsuIsValid {
                IconButton(
                    modifier = Modifier.padding(end = 16.dp),
                    onClick = { showTopPopup.value = true },
                    holdDownState = showTopPopup.value
                ) {
                    Icon(
                        imageVector = MiuixIcons.Useful.Reboot,
                        contentDescription = stringResource(id = R.string.reboot),
                        tint = colorScheme.onBackground
                    )
                }
                ListPopup(
                    show = showTopPopup,
                    popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                    alignment = PopupPositionProvider.Align.TopRight,
                    onDismissRequest = {
                        showTopPopup.value = false
                    }
                ) {
                    val pm = LocalContext.current.getSystemService(Context.POWER_SERVICE) as PowerManager?

                    @Suppress("DEPRECATION")
                    val isRebootingUserspaceSupported =
                        Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && pm?.isRebootingUserspaceSupported == true

                    ListPopupColumn {
                        val rebootOptions = mutableListOf(
                            Pair(R.string.reboot, ""),
                            Pair(R.string.reboot_recovery, "recovery"),
                            Pair(R.string.reboot_bootloader, "bootloader"),
                            Pair(R.string.reboot_download, "download"),
                            Pair(R.string.reboot_edl, "edl")
                        )
                        if (isRebootingUserspaceSupported) {
                            rebootOptions.add(1, Pair(R.string.reboot_userspace, "userspace"))
                        }
                        rebootOptions.forEachIndexed { idx, (id, reason) ->
                            RebootDropdownItem(
                                id = id,
                                reason = reason,
                                showTopPopup = showTopPopup,
                                optionSize = rebootOptions.size,
                                index = idx
                            )
                        }
                    }
                }
            }
        },
        scrollBehavior = scrollBehavior
    )
}

@Composable
private fun StatusCard(
    kernelVersion: KernelVersion,
    ksuVersion: Int?,
    lkmMode: Boolean?,
    onClickInstall: () -> Unit = {},
    onClickSuperuser: () -> Unit = {},
    onclickModule: () -> Unit = {},
) {
    Column(
        modifier = Modifier
    ) {
        when {
            ksuVersion != null -> {
                val safeMode = when {
                    Natives.isSafeMode -> " [${stringResource(id = R.string.safe_mode)}]"
                    else -> ""
                }

                val workingMode = when (lkmMode) {
                    null -> ""
                    true -> " <LKM>"
                    else -> " <GKI>"
                }

                val workingText = "${stringResource(id = R.string.home_working)}$workingMode$safeMode"

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
                            color = if (isSystemInDarkTheme()) Color(0xFF1A3825) else Color(0xFFDFFAE4)
                        ),
                        onClick = {
                            if (kernelVersion.isGKI()) onClickInstall()
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
                                    tint = Color(0xFF36D167),
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
                                    text = getSuperuserCount().toString(),
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
                                    text = getModuleCount().toString(),
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
                Card(
                    onClick = {
                        if (kernelVersion.isGKI()) onClickInstall()
                    },
                    showIndication = true,
                    pressFeedbackType = PressFeedbackType.Sink
                ) {
                    BasicComponent(
                        title = stringResource(R.string.home_not_installed),
                        summary = stringResource(R.string.home_click_to_install),
                        leftAction = {
                            Icon(
                                Icons.Rounded.ErrorOutline,
                                stringResource(R.string.home_not_installed),
                                modifier = Modifier
                                    .padding(end = 16.dp),
                                tint = colorScheme.onBackground,
                            )
                        }
                    )
                }
            }

            else -> {
                Card(
                    onClick = {
                        if (kernelVersion.isGKI()) onClickInstall()
                    },
                    showIndication = true,
                    pressFeedbackType = PressFeedbackType.Sink
                ) {
                    BasicComponent(
                        title = stringResource(R.string.home_unsupported),
                        summary = stringResource(R.string.home_unsupported_reason),
                        leftAction = {
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
fun WarningCard(
    message: String,
    color: Color = if (isSystemInDarkTheme()) Color(0XFF310808) else Color(0xFFF8E2E2),
    onClick: (() -> Unit)? = null
) {
    Card(
        onClick = {
            onClick?.invoke()
        },
        colors = CardDefaults.defaultColors(
            color = color
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
                color = Color(0xFFF72727),
                fontSize = 14.sp
            )
        }
    }
}

@Composable
fun LearnMoreCard() {
    val uriHandler = LocalUriHandler.current
    val url = stringResource(R.string.home_learn_kernelsu_url)

    Card(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        BasicComponent(
            title = stringResource(R.string.home_learn_kernelsu),
            summary = stringResource(R.string.home_click_to_learn_kernelsu),
            rightActions = {
                Icon(
                    modifier = Modifier.size(28.dp),
                    imageVector = Icons.Rounded.Link,
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
fun DonateCard() {
    val uriHandler = LocalUriHandler.current

    Card(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        BasicComponent(
            title = stringResource(R.string.home_support_title),
            summary = stringResource(R.string.home_support_content),
            rightActions = {
                Icon(
                    modifier = Modifier.size(28.dp),
                    imageVector = Icons.Rounded.Link,
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
private fun InfoCard() {
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
        val context = LocalContext.current
        val uname = Os.uname()
        val managerVersion = getManagerVersion(context)
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            InfoText(
                title = stringResource(R.string.home_kernel),
                content = uname.release
            )
            InfoText(
                title = stringResource(R.string.home_manager_version),
                content = "${managerVersion.first} (${managerVersion.second})"
            )
            InfoText(
                title = stringResource(R.string.home_fingerprint),
                content = Build.FINGERPRINT
            )
            InfoText(
                title = stringResource(R.string.home_selinux_status),
                content = getSELinuxStatus(),
                bottomPadding = 0.dp
            )
        }
    }
}

fun getManagerVersion(context: Context): Pair<String, Long> {
    val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)!!
    val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo)
    return Pair(packageInfo.versionName!!, versionCode)
}
