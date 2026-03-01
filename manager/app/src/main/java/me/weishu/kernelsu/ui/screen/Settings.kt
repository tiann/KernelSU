package me.weishu.kernelsu.ui.screen

import android.os.Build
import androidx.activity.compose.LocalActivity
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Adb
import androidx.compose.material.icons.rounded.AspectRatio
import androidx.compose.material.icons.rounded.BlurOn
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.CallToAction
import androidx.compose.material.icons.rounded.Colorize
import androidx.compose.material.icons.rounded.ContactPage
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.DeleteForever
import androidx.compose.material.icons.rounded.DeveloperMode
import androidx.compose.material.icons.rounded.Fence
import androidx.compose.material.icons.rounded.FolderDelete
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.RemoveCircle
import androidx.compose.material.icons.rounded.RemoveModerator
import androidx.compose.material.icons.rounded.RestartAlt
import androidx.compose.material.icons.rounded.Update
import androidx.compose.material.icons.rounded.UploadFile
import androidx.compose.material.icons.rounded.WaterDrop
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import me.weishu.kernelsu.KernelSUApplication
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.ScaleDialog
import me.weishu.kernelsu.ui.component.SendLogDialog
import me.weishu.kernelsu.ui.component.UninstallDialog
import me.weishu.kernelsu.ui.component.rememberLoadingDialog
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.viewmodel.SettingsViewModel
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Slider
import top.yukonga.miuix.kmp.basic.SliderDefaults
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperDropdown
import top.yukonga.miuix.kmp.extra.SuperSwitch
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic
import kotlin.math.roundToInt

/**
 * @author weishu
 * @date 2023/1/1.
 */
@Composable
fun SettingPager(
    navigator: Navigator,
    bottomInnerPadding: Dp
) {
    val context = LocalContext.current
    val viewModel = viewModel<SettingsViewModel>()
    val uiState by viewModel.uiState.collectAsState()

    val enableBlur = uiState.enableBlur

    val scrollBehavior = MiuixScrollBehavior()
    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = colorScheme.surface,
        tint = HazeTint(colorScheme.surface.copy(0.8f))
    )

    Scaffold(
        topBar = {
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
                title = stringResource(R.string.settings),
                scrollBehavior = scrollBehavior
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        val activity = LocalActivity.current

        val loadingDialog = rememberLoadingDialog()
        val showScaleDialog = rememberSaveable { mutableStateOf(false) }
        val showUninstallDialog = rememberSaveable { mutableStateOf(false) }
        val showSendLogDialog = rememberSaveable { mutableStateOf(false) }

        LazyColumn(
            modifier = Modifier
                .fillMaxHeight()
                .scrollEndHaptic()
                .overScrollVertical()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .hazeSource(state = hazeState)
                .padding(horizontal = 12.dp),
            contentPadding = innerPadding,
            overscrollEffect = null,
        ) {
            item {
                Card(
                    modifier = Modifier
                        .padding(top = 12.dp)
                        .fillMaxWidth(),
                ) {
                    SuperSwitch(
                        title = stringResource(id = R.string.settings_check_update),
                        summary = stringResource(id = R.string.settings_check_update_summary),
                        startAction = {
                            Icon(
                                Icons.Rounded.Update,
                                modifier = Modifier.padding(end = 6.dp),
                                contentDescription = stringResource(id = R.string.settings_check_update),
                                tint = colorScheme.onBackground
                            )
                        },
                        checked = uiState.checkUpdate,
                        onCheckedChange = {
                            viewModel.setCheckUpdate(it)
                        }
                    )
                    KsuIsValid {
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_module_check_update),
                            summary = stringResource(id = R.string.settings_check_update_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.UploadFile,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_check_update),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.checkModuleUpdate,
                            onCheckedChange = {
                                viewModel.setCheckModuleUpdate(it)
                            }
                        )
                    }
                }

                Card(
                    modifier = Modifier
                        .padding(top = 12.dp)
                        .fillMaxWidth(),
                ) {
                    val themeItems = listOf(
                        stringResource(id = R.string.settings_theme_mode_system),
                        stringResource(id = R.string.settings_theme_mode_light),
                        stringResource(id = R.string.settings_theme_mode_dark),
                        stringResource(id = R.string.settings_theme_mode_monet_system),
                        stringResource(id = R.string.settings_theme_mode_monet_light),
                        stringResource(id = R.string.settings_theme_mode_monet_dark),
                    )
                    SuperDropdown(
                        title = stringResource(id = R.string.settings_theme),
                        summary = stringResource(id = R.string.settings_theme_summary),
                        items = themeItems,
                        startAction = {
                            Icon(
                                Icons.Rounded.Palette,
                                modifier = Modifier.padding(end = 6.dp),
                                contentDescription = stringResource(id = R.string.settings_theme),
                                tint = colorScheme.onBackground
                            )
                        },
                        selectedIndex = uiState.themeMode,
                        onSelectedIndexChange = { index ->
                            viewModel.setThemeMode(index)
                        }
                    )

                    AnimatedVisibility(
                        visible = uiState.themeMode in 3..5
                    ) {
                        val colorItems = listOf(
                            stringResource(id = R.string.settings_key_color_default),
                            stringResource(id = R.string.color_red),
                            stringResource(id = R.string.color_pink),
                            stringResource(id = R.string.color_purple),
                            stringResource(id = R.string.color_deep_purple),
                            stringResource(id = R.string.color_indigo),
                            stringResource(id = R.string.color_blue),
                            stringResource(id = R.string.color_cyan),
                            stringResource(id = R.string.color_teal),
                            stringResource(id = R.string.color_green),
                            stringResource(id = R.string.color_yellow),
                            stringResource(id = R.string.color_amber),
                            stringResource(id = R.string.color_orange),
                            stringResource(id = R.string.color_brown),
                            stringResource(id = R.string.color_blue_grey),
                            stringResource(id = R.string.color_sakura),
                        )
                        val colorValues = listOf(
                            0,
                            Color(0xFFF44336).toArgb(),
                            Color(0xFFE91E63).toArgb(),
                            Color(0xFF9C27B0).toArgb(),
                            Color(0xFF673AB7).toArgb(),
                            Color(0xFF3F51B5).toArgb(),
                            Color(0xFF2196F3).toArgb(),
                            Color(0xFF00BCD4).toArgb(),
                            Color(0xFF009688).toArgb(),
                            Color(0xFF4FAF50).toArgb(),
                            Color(0xFFFFEB3B).toArgb(),
                            Color(0xFFFFC107).toArgb(),
                            Color(0xFFFF9800).toArgb(),
                            Color(0xFF795548).toArgb(),
                            Color(0xFF607D8F).toArgb(),
                            Color(0xFFFF9CA8).toArgb(),
                        )
                        val keyColorIndex = colorValues.indexOf(uiState.keyColor).takeIf { it >= 0 } ?: 0

                        SuperDropdown(
                            title = stringResource(id = R.string.settings_key_color),
                            summary = stringResource(id = R.string.settings_key_color_summary),
                            items = colorItems,
                            startAction = {
                                Icon(
                                    Icons.Rounded.Colorize,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_key_color),
                                    tint = colorScheme.onBackground
                                )
                            },
                            selectedIndex = keyColorIndex,
                            onSelectedIndexChange = { index ->
                                viewModel.setKeyColor(colorValues[index])
                            }
                        )
                    }

                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_enable_predictive_back),
                            summary = stringResource(id = R.string.settings_enable_predictive_back_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.Adb,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_enable_predictive_back),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.enablePredictiveBack,
                            onCheckedChange = {
                                viewModel.setEnablePredictiveBack(it)
                                KernelSUApplication.setEnableOnBackInvokedCallback(context.applicationInfo, it)
                                activity?.recreate()
                            }
                        )
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_enable_blur),
                            summary = stringResource(id = R.string.settings_enable_blur_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.WaterDrop,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_enable_blur),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.enableBlur,
                            onCheckedChange = {
                                viewModel.setEnableBlur(it)
                            }
                        )
                    }
                    SuperSwitch(
                        title = stringResource(id = R.string.settings_floating_bottom_bar),
                        summary = stringResource(id = R.string.settings_floating_bottom_bar_summary),
                        startAction = {
                            Icon(
                                Icons.Rounded.CallToAction,
                                modifier = Modifier.padding(end = 6.dp),
                                contentDescription = stringResource(id = R.string.settings_floating_bottom_bar),
                                tint = colorScheme.onBackground
                            )
                        },
                        checked = uiState.enableFloatingBottomBar,
                        onCheckedChange = {
                            viewModel.setEnableFloatingBottomBar(it)
                        }
                    )
                    AnimatedVisibility(visible = uiState.enableFloatingBottomBar && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_enable_glass),
                            summary = stringResource(id = R.string.settings_enable_glass_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.BlurOn,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_enable_glass),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.enableFloatingBottomBarBlur,
                            onCheckedChange = {
                                viewModel.setEnableFloatingBottomBarBlur(it)
                            }
                        )
                    }
                    var sliderValue by remember(uiState.pageScale) { mutableFloatStateOf(uiState.pageScale) }
                    SuperArrow(
                        title = stringResource(id = R.string.settings_page_scale),
                        summary = stringResource(id = R.string.settings_page_scale_summary),
                        startAction = {
                            Icon(
                                Icons.Rounded.AspectRatio,
                                modifier = Modifier.padding(end = 6.dp),
                                contentDescription = stringResource(id = R.string.settings_page_scale),
                                tint = colorScheme.onBackground
                            )
                        },
                        endActions = {
                            Text(
                                text = "${(sliderValue * 100).toInt()}%",
                                color = colorScheme.onSurfaceVariantActions,
                            )
                        },
                        onClick = { showScaleDialog.value = !showScaleDialog.value },
                        holdDownState = showScaleDialog.value,
                        bottomAction = {
                            Slider(
                                value = sliderValue,
                                onValueChange = {
                                    sliderValue = it
                                },
                                onValueChangeFinished = {
                                    val scale = (sliderValue * 100).roundToInt() / 100f
                                    sliderValue = scale
                                    viewModel.setPageScale(scale)
                                },
                                valueRange = 0.8f..1.1f,
                                showKeyPoints = true,
                                keyPoints = listOf(0.8f, 0.9f, 1f, 1.1f),
                                magnetThreshold = 0.01f,
                                hapticEffect = SliderDefaults.SliderHapticEffect.Step,
                            )
                        },
                    )
                    ScaleDialog(
                        showScaleDialog,
                        volumeState = { uiState.pageScale },
                        onVolumeChange = {
                            viewModel.setPageScale(it)
                        }
                    )
                }

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        val profileTemplate = stringResource(id = R.string.settings_profile_template)
                        SuperArrow(
                            title = profileTemplate,
                            summary = stringResource(id = R.string.settings_profile_template_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.Fence,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = profileTemplate,
                                    tint = colorScheme.onBackground
                                )
                            },
                            onClick = {
                                navigator.push(Route.AppProfileTemplate)
                            }
                        )
                    }
                }

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        val suCompatModeItems = listOf(
                            stringResource(id = R.string.settings_mode_enable_by_default),
                            stringResource(id = R.string.settings_mode_disable_until_reboot),
                            stringResource(id = R.string.settings_mode_disable_always),
                        )

                        val suSummary = when (uiState.suCompatStatus) {
                            "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                            "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                            else -> stringResource(id = R.string.settings_sucompat_summary)
                        }
                        SuperDropdown(
                            title = stringResource(id = R.string.settings_sucompat),
                            summary = suSummary,
                            items = suCompatModeItems,
                            startAction = {
                                Icon(
                                    Icons.Rounded.RemoveModerator,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_sucompat),
                                    tint = colorScheme.onBackground
                                )
                            },
                            enabled = uiState.suCompatStatus == "supported",
                            selectedIndex = uiState.suCompatMode,
                            onSelectedIndexChange = { index ->
                                viewModel.setSuCompatMode(index)
                            }
                        )

                        val umountSummary = when (uiState.kernelUmountStatus) {
                            "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                            "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                            else -> stringResource(id = R.string.settings_kernel_umount_summary)
                        }
                        SuperSwitch(
                            title = stringResource(id = R.string.settings_kernel_umount),
                            summary = umountSummary,
                            startAction = {
                                Icon(
                                    Icons.Rounded.RemoveCircle,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_kernel_umount),
                                    tint = colorScheme.onBackground
                                )
                            },
                            enabled = uiState.kernelUmountStatus == "supported",
                            checked = uiState.isKernelUmountEnabled,
                            onCheckedChange = { checked ->
                                viewModel.setKernelUmountEnabled(checked)
                            }
                        )

                        SuperSwitch(
                            title = stringResource(id = R.string.settings_umount_modules_default),
                            summary = stringResource(id = R.string.settings_umount_modules_default_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.FolderDelete,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_umount_modules_default),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.isDefaultUmountModules,
                            onCheckedChange = {
                                viewModel.setDefaultUmountModules(it)
                            }
                        )

                        SuperSwitch(
                            title = stringResource(id = R.string.enable_web_debugging),
                            summary = stringResource(id = R.string.enable_web_debugging_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.DeveloperMode,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.enable_web_debugging),
                                    tint = colorScheme.onBackground
                                )
                            },
                            checked = uiState.enableWebDebugging,
                            onCheckedChange = {
                                viewModel.setEnableWebDebugging(it)
                            }
                        )
                    }
                }

                KsuIsValid {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        if (uiState.isLkmMode) {
                            val uninstall = stringResource(id = R.string.settings_uninstall)
                            SuperArrow(
                                title = uninstall,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.Delete,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = uninstall,
                                        tint = colorScheme.onBackground,
                                    )
                                },
                                onClick = {
                                    showUninstallDialog.value = true
                                }
                            )
                            UninstallDialog(showUninstallDialog, navigator)
                        }
                    }
                }

                Card(
                    modifier = Modifier
                        .padding(vertical = 12.dp)
                        .fillMaxWidth(),
                ) {
                    SuperArrow(
                        title = stringResource(id = R.string.send_log),
                        startAction = {
                            Icon(
                                Icons.Rounded.BugReport,
                                modifier = Modifier.padding(end = 6.dp),
                                contentDescription = stringResource(id = R.string.send_log),
                                tint = colorScheme.onBackground
                            )
                        },
                        onClick = {
                            showSendLogDialog.value = true
                        },
                    )
                    SendLogDialog(showSendLogDialog, loadingDialog)
                    val about = stringResource(id = R.string.about)
                    SuperArrow(
                        title = about,
                        startAction = {
                            Icon(
                                Icons.Rounded.ContactPage,
                                modifier = Modifier.padding(end = 6.dp),
                                contentDescription = about,
                                tint = colorScheme.onBackground
                            )
                        },
                        onClick = {
                            navigator.push(Route.About)
                        }
                    )
                }
                Spacer(Modifier.height(bottomInnerPadding))
            }
        }
    }
}

enum class UninstallType(val icon: ImageVector, val title: Int, val message: Int) {
    TEMPORARY(
        Icons.Rounded.RemoveModerator,
        R.string.settings_uninstall_temporary,
        R.string.settings_uninstall_temporary_message
    ),
    PERMANENT(
        Icons.Rounded.DeleteForever,
        R.string.settings_uninstall_permanent,
        R.string.settings_uninstall_permanent_message
    ),
    RESTORE_STOCK_IMAGE(
        Icons.Rounded.RestartAlt,
        R.string.settings_restore_stock_image,
        R.string.settings_restore_stock_image_message
    ),
    NONE(Icons.Rounded.Adb, 0, 0)
}
