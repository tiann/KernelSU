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
import androidx.activity.compose.BackHandler
import androidx.activity.compose.LocalActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import androidx.core.util.Consumer
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.NavGraphs
import com.ramcosta.composedestinations.generated.destinations.AboutScreenDestination
import com.ramcosta.composedestinations.generated.destinations.AppProfileScreenDestination
import com.ramcosta.composedestinations.generated.destinations.AppProfileTemplateScreenDestination
import com.ramcosta.composedestinations.generated.destinations.ExecuteModuleActionScreenDestination
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import com.ramcosta.composedestinations.generated.destinations.InstallScreenDestination
import com.ramcosta.composedestinations.generated.destinations.MainScreenDestination
import com.ramcosta.composedestinations.generated.destinations.ModuleRepoDetailScreenDestination
import com.ramcosta.composedestinations.generated.destinations.ModuleRepoScreenDestination
import com.ramcosta.composedestinations.generated.destinations.SettingPagerDestination
import com.ramcosta.composedestinations.generated.destinations.TemplateEditorScreenDestination
import com.ramcosta.composedestinations.generated.destinations.TemplateScreenDestination
import com.ramcosta.composedestinations.navargs.primitives.booleanNavType
import com.ramcosta.composedestinations.rememberNavHostEngine
import com.ramcosta.composedestinations.scope.resultBackNavigator
import com.ramcosta.composedestinations.scope.resultRecipient
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.BottomBar
import me.weishu.kernelsu.ui.component.navigation.MiuixDestinationsNavigator
import me.weishu.kernelsu.ui.component.navigation.PopTransitionStyle
import me.weishu.kernelsu.ui.component.navigation.SharedDestinationsNavHost
import me.weishu.kernelsu.ui.component.navigation.miuixComposable
import me.weishu.kernelsu.ui.component.navigation.miuixDestinationsNavigator
import me.weishu.kernelsu.ui.component.navigation.noAnimated
import me.weishu.kernelsu.ui.component.navigation.slideFromRightTransition
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
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
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.util.install
import me.weishu.kernelsu.ui.webui.WebUIActivity
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.theme.MiuixTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)

        val isManager = Natives.isManager
        if (isManager && !Natives.requireNewKernel()) install()

        setContent {
            val context = LocalActivity.current ?: this
            val prefs = context.getSharedPreferences("settings", MODE_PRIVATE)
            var colorMode by remember { mutableIntStateOf(prefs.getInt("color_mode", 0)) }
            var keyColorInt by remember { mutableIntStateOf(prefs.getInt("key_color", 0)) }
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
                    }
                }
                prefs.registerOnSharedPreferenceChangeListener(listener)
                onDispose { prefs.unregisterOnSharedPreferenceChangeListener(listener) }
            }

            KernelSUTheme(colorMode = colorMode, keyColor = keyColor) {
                Scaffold {
                    val engine = rememberNavHostEngine()
                    val navController = engine.rememberNavController()
                    val pagerState = rememberPagerState(initialPage = 0, pageCount = { 4 })
                    val coroutineScope = rememberCoroutineScope()

                    BackHandler(enabled = true) {
                        if (pagerState.currentPage != 0) {
                            coroutineScope.launch {
                                pagerState.animateScrollToPage(0)
                            }
                        } else {
                            this.moveTaskToBack(true)
                        }
                    }

                    CompositionLocalProvider(
                        LocalPagerState provides pagerState
                    ) {
                        SharedDestinationsNavHost(
                            engine = engine,
                            navGraph = NavGraphs.root,
                            navController = navController,
                            overlayContent = {
                                // Handle ZIP file installation from external apps
                                ZipFileIntentHandler(isManager = isManager, navigator = this)

                                // Handle shortcut intents
                                ShortcutIntentHandler(navigator = this)
                            }
                        ) {
                            miuixComposable(MainScreenDestination) { MainScreen(miuixDestinationsNavigator()) }
                            miuixComposable(AboutScreenDestination) { AboutScreen(miuixDestinationsNavigator()) }
                            miuixComposable(InstallScreenDestination) { InstallScreen(miuixDestinationsNavigator()) }
                            miuixComposable(AppProfileScreenDestination) { AppProfileScreen(miuixDestinationsNavigator(), navArgs.appInfo) }
                            miuixComposable(SettingPagerDestination) { SettingPager(miuixDestinationsNavigator(), 0.dp) }
                            miuixComposable(ExecuteModuleActionScreenDestination) {
                                ExecuteModuleActionScreen(
                                    miuixDestinationsNavigator(),
                                    navArgs.moduleId,
                                    navArgs.fromShortcut
                                )
                            }
                            miuixComposable(FlashScreenDestination) { FlashScreen(miuixDestinationsNavigator(), navArgs.flashIt) }
                            miuixComposable(AppProfileTemplateScreenDestination, slideFromRightTransition, PopTransitionStyle.Depth) {
                                AppProfileTemplateScreen(
                                    navigator = miuixDestinationsNavigator(),
                                    resultRecipient = resultRecipient(booleanNavType)
                                )
                            }
                            miuixComposable(ModuleRepoScreenDestination, slideFromRightTransition, PopTransitionStyle.Depth) {
                                ModuleRepoScreen(
                                    miuixDestinationsNavigator(),
                                    this@miuixComposable
                                )
                            }
                            miuixComposable(ModuleRepoDetailScreenDestination, noAnimated) {
                                val (module) = navArgs
                                ModuleRepoDetailScreen(
                                    navigator = miuixDestinationsNavigator(),
                                    animatedVisibilityScope = this@miuixComposable,
                                    module = module
                                )
                            }
                            miuixComposable(TemplateScreenDestination) {
                                val (initialTemplate, transitionSource, readOnly) = navArgs
                                TemplateEditorScreen(
                                    navigator = resultBackNavigator(booleanNavType),
                                    animatedVisibilityScope = this@miuixComposable,
                                    initialTemplate = initialTemplate,
                                    transitionSource = transitionSource,
                                    readOnly = readOnly
                                )
                            }
                            miuixComposable(TemplateEditorScreenDestination, noAnimated) {
                                val (initialTemplate, transitionSource, readOnly) = navArgs
                                TemplateEditorScreen(
                                    navigator = resultBackNavigator(booleanNavType),
                                    animatedVisibilityScope = this@miuixComposable,
                                    initialTemplate = initialTemplate,
                                    transitionSource = transitionSource,
                                    readOnly = readOnly
                                )
                            }
                        }
                    }
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
    }
}

val LocalPagerState = compositionLocalOf<PagerState> { error("No pager state") }

@Composable
@Destination<RootGraph>(start = true)
fun MainScreen(navController: MiuixDestinationsNavigator) {
    val pagerState = LocalPagerState.current
    val isManager = Natives.isManager
    val isFullFeatured by remember { derivedStateOf { isManager && !Natives.requireNewKernel() } }
    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = MiuixTheme.colorScheme.surface,
        tint = HazeTint(MiuixTheme.colorScheme.surface.copy(0.8f))
    )

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        bottomBar = {
            BottomBar(hazeState, hazeStyle)
        },
    ) { innerPadding ->
        HorizontalPager(
            modifier = Modifier.hazeSource(state = hazeState),
            state = pagerState,
            beyondViewportPageCount = 1,
            userScrollEnabled = isFullFeatured,
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

/**
 * Handles ZIP file installation from external apps (e.g., file managers).
 * - In normal mode: Shows a confirmation dialog before installation
 * - In safe mode: Shows a Toast notification and prevents installation
 */
@SuppressLint("StringFormatInvalid")
@Composable
private fun ZipFileIntentHandler(
    isManager: Boolean,
    navigator: MiuixDestinationsNavigator
) {
    val activity = LocalActivity.current as ComponentActivity
    val context = LocalContext.current
    var zipUri by remember { mutableStateOf<Uri?>(null) }
    val isSafeMode = Natives.isSafeMode
    val clearZipUri = { zipUri = null }

    val installDialog = rememberConfirmDialog(
        onConfirm = {
            zipUri?.let { uri ->
                navigator.navigate(
                    FlashScreenDestination(FlashIt.FlashModules(listOf(uri)))
                )
            }
            clearZipUri()
        },
        onDismiss = clearZipUri
    )

    fun getDisplayName(uri: Uri): String {
        return uri.getFileName(activity) ?: uri.lastPathSegment ?: "Unknown"
    }

    fun handleIntent(intent: Intent) {
        try {
            val uri = intent.data ?: return
            if (!isManager || uri.scheme != "content" || intent.type != "application/zip") return

            if (isSafeMode) {
                Toast.makeText(
                    context,
                    activity.getString(R.string.safe_mode_module_disabled),
                    Toast.LENGTH_SHORT
                ).show()
            } else {
                zipUri = uri
                installDialog.showConfirm(
                    title = activity.getString(R.string.module),
                    content = activity.getString(
                        R.string.module_install_prompt_with_name,
                        "\n${getDisplayName(uri)}"
                    )
                )
            }
        } catch (_: Exception) {
            Toast.makeText(
                context,
                activity.getString(R.string.module_install_intent_failed),
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    LaunchedEffect(Unit) {
        handleIntent(activity.intent)
    }

    DisposableEffect(Unit) {
        val listener = Consumer<Intent> { intent ->
            handleIntent(intent)
        }
        activity.addOnNewIntentListener(listener)

        onDispose {
            activity.removeOnNewIntentListener(listener)
        }
    }
}

@Composable
private fun ShortcutIntentHandler(
    navigator: MiuixDestinationsNavigator
) {
    val activity = LocalActivity.current as ComponentActivity
    val context = LocalContext.current

    fun handleIntent(intent: Intent) {
        val type = intent.getStringExtra("shortcut_type") ?: return
        when (type) {
            "module_action" -> {
                val moduleId = intent.getStringExtra("module_id") ?: return
                navigator.navigate(ExecuteModuleActionScreenDestination(moduleId, fromShortcut = true)) {
                    launchSingleTop = true
                }
            }

            "module_webui" -> {
                val moduleId = intent.getStringExtra("module_id") ?: return
                val webIntent = Intent(context, WebUIActivity::class.java)
                    .setData("kernelsu://webui/$moduleId".toUri())
                context.startActivity(webIntent)
            }
        }
    }

    LaunchedEffect(Unit) {
        handleIntent(activity.intent)
    }

    DisposableEffect(Unit) {
        val listener = Consumer<Intent> { intent ->
            handleIntent(intent)
        }
        activity.addOnNewIntentListener(listener)

        onDispose {
            activity.removeOnNewIntentListener(listener)
        }
    }
}
