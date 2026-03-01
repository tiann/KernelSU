package me.weishu.kernelsu.ui

import android.annotation.SuppressLint
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.LocalActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Density
import androidx.core.net.toUri
import androidx.lifecycle.viewmodel.navigation3.rememberViewModelStoreNavEntryDecorator
import androidx.navigation3.runtime.entryProvider
import androidx.navigation3.runtime.rememberSaveableStateHolderNavEntryDecorator
import androidx.navigation3.ui.NavDisplay
import androidx.navigationevent.NavigationEventInfo
import androidx.navigationevent.compose.NavigationBackHandler
import androidx.navigationevent.compose.rememberNavigationEventState
import com.kyant.backdrop.backdrops.layerBackdrop
import com.kyant.backdrop.backdrops.rememberLayerBackdrop
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.flow.MutableStateFlow
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.BottomBar
import me.weishu.kernelsu.ui.component.MainPagerState
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.rememberMainPagerState
import me.weishu.kernelsu.ui.navigation3.HandleDeepLink
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Navigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.navigation3.rememberNavigator
import me.weishu.kernelsu.ui.screen.AboutScreen
import me.weishu.kernelsu.ui.screen.AppProfileScreen
import me.weishu.kernelsu.ui.screen.AppProfileTemplateScreen
import me.weishu.kernelsu.ui.screen.ExecuteModuleActionScreen
import me.weishu.kernelsu.ui.screen.FlashIt
import me.weishu.kernelsu.ui.screen.FlashScreen
import me.weishu.kernelsu.ui.screen.HomePager
import me.weishu.kernelsu.ui.screen.InstallScreen
import me.weishu.kernelsu.ui.screen.ModulePager
import me.weishu.kernelsu.ui.screen.ModuleRepoDetailScreen
import me.weishu.kernelsu.ui.screen.ModuleRepoScreen
import me.weishu.kernelsu.ui.screen.SettingPager
import me.weishu.kernelsu.ui.screen.SuperUserPager
import me.weishu.kernelsu.ui.screen.TemplateEditorScreen
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.theme.LocalColorMode
import me.weishu.kernelsu.ui.theme.LocalEnableBlur
import me.weishu.kernelsu.ui.theme.LocalEnableFloatingBottomBar
import me.weishu.kernelsu.ui.theme.LocalEnableFloatingBottomBarBlur
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.util.install
import me.weishu.kernelsu.ui.webui.WebUIActivity
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.theme.MiuixTheme

class MainActivity : ComponentActivity() {

    private val intentState = MutableStateFlow(0)

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)

        val isManager = Natives.isManager
        if (isManager && !Natives.requireNewKernel()) install()

        setContent {
            val context = LocalActivity.current ?: this
            val prefs = context.getSharedPreferences("settings", MODE_PRIVATE)
            var colorMode by remember { mutableIntStateOf(prefs.getInt("color_mode", 0)) }
            var keyColorInt by remember { mutableIntStateOf(prefs.getInt("key_color", 0)) }
            var pageScale by remember { mutableFloatStateOf(prefs.getFloat("page_scale", 1f)) }
            var enableBlur by remember { mutableStateOf(prefs.getBoolean("enable_blur", true)) }
            var enableFloatingBottomBar by remember { mutableStateOf(prefs.getBoolean("enable_floating_bottom_bar", false)) }
            var enableFloatingBottomBarBlur by remember { mutableStateOf(prefs.getBoolean("enable_floating_bottom_bar_blur", true)) }


            val keyColor = remember(keyColorInt) { if (keyColorInt == 0) null else Color(keyColorInt) }

            val darkMode = when (colorMode) {
                2, 5 -> true
                0, 3 -> isSystemInDarkTheme()
                else -> false
            }

            DisposableEffect(prefs, darkMode) {
                enableEdgeToEdge(
                    statusBarStyle = SystemBarStyle.auto(
                        android.graphics.Color.TRANSPARENT,
                        android.graphics.Color.TRANSPARENT
                    ) { darkMode },
                    navigationBarStyle = SystemBarStyle.auto(
                        android.graphics.Color.TRANSPARENT,
                        android.graphics.Color.TRANSPARENT
                    ) { darkMode },
                )
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    window.isNavigationBarContrastEnforced = false
                }

                val listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
                    when (key) {
                        "color_mode" -> colorMode = prefs.getInt("color_mode", 0)
                        "key_color" -> keyColorInt = prefs.getInt("key_color", 0)
                        "page_scale" -> pageScale = prefs.getFloat("page_scale", 1f)
                        "enable_blur" -> enableBlur = prefs.getBoolean("enable_blur", true)
                        "enable_floating_bottom_bar" -> enableFloatingBottomBar = prefs.getBoolean("enable_floating_bottom_bar", false)
                        "enable_floating_bottom_bar_blur" -> enableFloatingBottomBarBlur =
                            prefs.getBoolean("enable_floating_bottom_bar_blur", true)
                    }
                }
                prefs.registerOnSharedPreferenceChangeListener(listener)
                onDispose { prefs.unregisterOnSharedPreferenceChangeListener(listener) }
            }

            val navigator = rememberNavigator(Route.Main)
            val systemDensity = LocalDensity.current
            val density = remember(systemDensity, pageScale) {
                Density(systemDensity.density * pageScale, systemDensity.fontScale)
            }
            CompositionLocalProvider(
                LocalNavigator provides navigator,
                LocalDensity provides density,
                LocalColorMode provides colorMode,
                LocalEnableBlur provides enableBlur,
                LocalEnableFloatingBottomBar provides enableFloatingBottomBar,
                LocalEnableFloatingBottomBarBlur provides enableFloatingBottomBarBlur,
            ) {
                KernelSUTheme(colorMode = colorMode, keyColor = keyColor) {

                    HandleDeepLink(
                        intentState = intentState.collectAsState(),
                    )

                    ZipFileIntentHandler(
                        intentState = intentState,
                        isManager = isManager,
                    )
                    ShortcutIntentHandler(
                        intentState = intentState,
                    )

                    Scaffold {
                        NavDisplay(
                            backStack = navigator.backStack,
                            entryDecorators = listOf(
                                rememberSaveableStateHolderNavEntryDecorator(),
                                rememberViewModelStoreNavEntryDecorator()
                            ),
                            onBack = {
                                when (val top = navigator.current()) {
                                    is Route.TemplateEditor -> {
                                        if (!top.readOnly) {
                                            navigator.setResult("template_edit", true)
                                        } else {
                                            navigator.pop()
                                        }
                                    }

                                    else -> navigator.pop()
                                }
                            },
                            entryProvider = entryProvider {
                                entry<Route.Main> { MainScreen() }
                                entry<Route.About> { AboutScreen() }
                                entry<Route.AppProfileTemplate> { AppProfileTemplateScreen() }
                                entry<Route.TemplateEditor> { key -> TemplateEditorScreen(key.template, key.readOnly) }
                                entry<Route.AppProfile> { key -> AppProfileScreen(key.uid, key.packageName) }
                                entry<Route.ModuleRepo> { ModuleRepoScreen() }
                                entry<Route.ModuleRepoDetail> { key -> ModuleRepoDetailScreen(key.module) }
                                entry<Route.Install> { InstallScreen() }
                                entry<Route.Flash> { key -> FlashScreen(key.flashIt) }
                                entry<Route.ExecuteModuleAction> { key -> ExecuteModuleActionScreen(key.moduleId) }
                                entry<Route.Home> { MainScreen() }
                                entry<Route.SuperUser> { MainScreen() }
                                entry<Route.Module> { MainScreen() }
                                entry<Route.Settings> { MainScreen() }
                            }
                        )
                    }
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        // Increment intentState to trigger LaunchedEffect re-execution
        intentState.value += 1
    }
}

val LocalMainPagerState = staticCompositionLocalOf<MainPagerState> { error("LocalMainPagerState not provided") }

@Composable
fun MainScreen() {
    val navController = LocalNavigator.current
    val enableBlur = LocalEnableBlur.current
    val enableFloatingBottomBar = LocalEnableFloatingBottomBar.current
    val enableFloatingBottomBarBlur = LocalEnableFloatingBottomBarBlur.current
    val pagerState = rememberPagerState(initialPage = 0, pageCount = { 4 })
    val mainPagerState = rememberMainPagerState(pagerState)
    val isManager = Natives.isManager
    val isFullFeatured = isManager && !Natives.requireNewKernel()
    var userScrollEnabled by remember(isFullFeatured) { mutableStateOf(isFullFeatured) }
    val surfaceColor = MiuixTheme.colorScheme.surface
    val hazeState = remember { HazeState() }
    val hazeStyle = if (enableBlur) {
        HazeStyle(
            backgroundColor = surfaceColor,
            tint = HazeTint(surfaceColor.copy(0.8f))
        )
    } else {
        HazeStyle.Unspecified
    }

    val backdrop = rememberLayerBackdrop {
        drawRect(surfaceColor)
        drawContent()
    }

    LaunchedEffect(mainPagerState.pagerState.currentPage) {
        mainPagerState.syncPage()
    }

    MainScreenBackHandler(mainPagerState, navController)

    CompositionLocalProvider(
        LocalMainPagerState provides mainPagerState
    ) {
        Scaffold(
            bottomBar = {
                Box(
                    modifier = Modifier.fillMaxWidth()
                ) {
                    BottomBar(
                        hazeState = hazeState,
                        hazeStyle = hazeStyle,
                        backdrop = backdrop,
                        modifier = Modifier.align(Alignment.BottomCenter)
                    )
                }
            },
        ) { innerPadding ->
            HorizontalPager(
                modifier = Modifier
                    .then(if (enableBlur) Modifier.hazeSource(state = hazeState) else Modifier)
                    .then(if (enableFloatingBottomBar && enableFloatingBottomBarBlur) Modifier.layerBackdrop(backdrop) else Modifier),
                state = mainPagerState.pagerState,
                beyondViewportPageCount = 3,
                userScrollEnabled = userScrollEnabled,
            ) {
                when (it) {
                    0 -> HomePager(navController, innerPadding.calculateBottomPadding())
                    1 -> SuperUserPager(navController, innerPadding.calculateBottomPadding())
                    2 -> ModulePager(navController, innerPadding.calculateBottomPadding())
                    3 -> SettingPager(navController, innerPadding.calculateBottomPadding())
                }
            }
        }
    }
}


@Composable
private fun MainScreenBackHandler(
    mainState: MainPagerState,
    navController: Navigator,
) {
    val isPagerBackHandlerEnabled by remember {
        derivedStateOf {
            navController.current() is Route.Main && navController.backStackSize() == 1 && mainState.selectedPage != 0
        }
    }

    val navEventState = rememberNavigationEventState(NavigationEventInfo.None)

    NavigationBackHandler(
        state = navEventState,
        isBackEnabled = isPagerBackHandlerEnabled,
        onBackCompleted = {
            mainState.animateToPage(0)
        }
    )
}

/**
 * Handles ZIP file installation from external apps (e.g., file managers).
 * - In normal mode: Shows a confirmation dialog before installation
 * - In safe mode: Shows a Toast notification and prevents installation
 */
@SuppressLint("StringFormatInvalid", "LocalContextGetResourceValueCall")
@Composable
private fun ZipFileIntentHandler(
    intentState: MutableStateFlow<Int>,
    isManager: Boolean,
) {
    val activity = LocalActivity.current ?: return
    val context = LocalContext.current
    var zipUri by remember { mutableStateOf<Uri?>(null) }
    val isSafeMode = Natives.isSafeMode
    val clearZipUri = { zipUri = null }
    val navigator = LocalNavigator.current

    val installDialog = rememberConfirmDialog(
        onConfirm = {
            zipUri?.let { uri ->
                navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri))))
            }
            clearZipUri()
        },
        onDismiss = clearZipUri
    )

    fun getDisplayName(uri: Uri): String {
        return uri.getFileName(context) ?: uri.lastPathSegment ?: "Unknown"
    }

    val intentStateValue by intentState.collectAsState()
    LaunchedEffect(intentStateValue) {
        val currentIntent = activity.intent
        val uri = currentIntent?.data ?: return@LaunchedEffect

        if (!isManager || uri.scheme != "content" || currentIntent.type != "application/zip") {
            return@LaunchedEffect
        }

        if (isSafeMode) {
            Toast.makeText(
                context,
                context.getString(R.string.safe_mode_module_disabled), Toast.LENGTH_SHORT
            )
                .show()
        } else {
            zipUri = uri
            installDialog.showConfirm(
                title = context.getString(R.string.module),
                content = context.getString(
                    R.string.module_install_prompt_with_name,
                    "\n${getDisplayName(uri)}"
                )
            )
        }
    }
}

@Composable
private fun ShortcutIntentHandler(
    intentState: MutableStateFlow<Int>,
) {
    val activity = LocalActivity.current ?: return
    val context = LocalContext.current
    val intentStateValue by intentState.collectAsState()
    val navigator = LocalNavigator.current
    LaunchedEffect(intentStateValue) {
        val intent = activity.intent
        val type = intent?.getStringExtra("shortcut_type") ?: return@LaunchedEffect
        when (type) {
            "module_action" -> {
                val moduleId = intent.getStringExtra("module_id") ?: return@LaunchedEffect
                navigator.push(Route.ExecuteModuleAction(moduleId))
            }

            "module_webui" -> {
                val moduleId = intent.getStringExtra("module_id") ?: return@LaunchedEffect
                val webIntent = Intent(context, WebUIActivity::class.java)
                    .setData("kernelsu://webui/$moduleId".toUri())
                context.startActivity(webIntent)
            }

            else -> return@LaunchedEffect
        }
    }
}
