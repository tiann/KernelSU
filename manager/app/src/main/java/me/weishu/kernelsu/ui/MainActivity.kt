package me.weishu.kernelsu.ui

import android.annotation.SuppressLint
import android.content.SharedPreferences
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.BackHandler
import androidx.activity.compose.LocalActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.navigation.NavBackStackEntry
import androidx.navigation.compose.rememberNavController
import com.ramcosta.composedestinations.DestinationsNavHost
import com.ramcosta.composedestinations.animations.NavHostAnimatedDestinationStyle
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.NavGraphs
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.ramcosta.composedestinations.utils.rememberDestinationsNavigator
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.BottomBar
import me.weishu.kernelsu.ui.component.rememberConfirmDialog
import me.weishu.kernelsu.ui.screen.FlashIt
import me.weishu.kernelsu.ui.screen.HomePager
import me.weishu.kernelsu.ui.screen.ModulePager
import me.weishu.kernelsu.ui.screen.SettingPager
import me.weishu.kernelsu.ui.screen.SuperUserPager
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.util.getFileName
import me.weishu.kernelsu.ui.util.install
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
                val navController = rememberNavController()
                val navigator = navController.rememberDestinationsNavigator()

                // Handle ZIP file installation from external apps
                ZipFileIntentHandler(
                    intentState = intentState,
                    intent = intent,
                    isManager = isManager,
                    navigator = navigator
                )

                Scaffold {
                    DestinationsNavHost(
                        modifier = Modifier,
                        navGraph = NavGraphs.root,
                        navController = navController,
                        defaultTransitions = object : NavHostAnimatedDestinationStyle() {
                            override val enterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition =
                                {
                                    slideInHorizontally(
                                        initialOffsetX = { it },
                                        animationSpec = tween(durationMillis = 500, easing = FastOutSlowInEasing)
                                    )
                                }

                            override val exitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition =
                                {
                                    slideOutHorizontally(
                                        targetOffsetX = { -it / 5 },
                                        animationSpec = tween(durationMillis = 500, easing = FastOutSlowInEasing)
                                    )
                                }

                            override val popEnterTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition =
                                {
                                    slideInHorizontally(
                                        initialOffsetX = { -it / 5 },
                                        animationSpec = tween(durationMillis = 500, easing = FastOutSlowInEasing)
                                    )
                                }

                            override val popExitTransition: AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition =
                                {
                                    slideOutHorizontally(
                                        targetOffsetX = { it },
                                        animationSpec = tween(durationMillis = 500, easing = FastOutSlowInEasing)
                                    )
                                }
                        }
                    )
                }
            }
        }
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        // Increment intentState to trigger LaunchedEffect re-execution
        intentState.value += 1
    }
}


val LocalPagerState = compositionLocalOf<PagerState> { error("No pager state") }
val LocalHandlePageChange = compositionLocalOf<(Int) -> Unit> { error("No handle page change") }
val LocalSelectedPage = compositionLocalOf<Int> { error("No selected page") }

@Composable
@Destination<RootGraph>(start = true)
fun MainScreen(navController: DestinationsNavigator) {
    val activity = LocalActivity.current
    val coroutineScope = rememberCoroutineScope()
    val pagerState = rememberPagerState(initialPage = 0, pageCount = { 4 })
    var userScrollEnabled by remember { mutableStateOf(true) }
    var animating by remember { mutableStateOf(false) }
    var uiSelectedPage by remember { mutableIntStateOf(0) }
    var animateJob by remember { mutableStateOf<Job?>(null) }
    var lastRequestedPage by remember { mutableIntStateOf(pagerState.currentPage) }
    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = MiuixTheme.colorScheme.surface,
        tint = HazeTint(MiuixTheme.colorScheme.surface.copy(0.8f))
    )
    val handlePageChange: (Int) -> Unit = remember(pagerState, coroutineScope) {
        { page ->
            uiSelectedPage = page
            if (page == pagerState.currentPage) {
                if (animateJob != null && lastRequestedPage != page) {
                    animateJob?.cancel()
                    animateJob = null
                    animating = false
                    userScrollEnabled = true
                }
                lastRequestedPage = page
            } else {
                if (animateJob != null && lastRequestedPage == page) {
                    // Already animating to the requested page
                } else {
                    animateJob?.cancel()
                    animating = true
                    userScrollEnabled = false
                    val job = coroutineScope.launch {
                        try {
                            pagerState.animateScrollToPage(page)
                        } finally {
                            if (animateJob === this) {
                                userScrollEnabled = true
                                animating = false
                                animateJob = null
                            }
                        }
                    }
                    animateJob = job
                    lastRequestedPage = page
                }
            }
        }
    }

    LaunchedEffect(pagerState) {
        snapshotFlow { pagerState.currentPage }.collect { page ->
            if (!animating) uiSelectedPage = page
        }
    }

    BackHandler {
        if (pagerState.currentPage != 0) {
            handlePageChange(0)
        } else {
            activity?.moveTaskToBack(true)
        }
    }

    CompositionLocalProvider(
        LocalPagerState provides pagerState,
        LocalHandlePageChange provides handlePageChange,
        LocalSelectedPage provides uiSelectedPage
    ) {
        Scaffold(
            bottomBar = {
                BottomBar(hazeState, hazeStyle)
            },
        ) { innerPadding ->
            HorizontalPager(
                modifier = Modifier.hazeSource(state = hazeState),
                state = pagerState,
                beyondViewportPageCount = 4,
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

/**
 * Handles ZIP file installation from external apps (e.g., file managers).
 * Shows a confirmation dialog to prevent accidental installation.
 */
@SuppressLint("StringFormatInvalid")
@Composable
private fun ZipFileIntentHandler(
    intentState: MutableStateFlow<Int>,
    intent: android.content.Intent?,
    isManager: Boolean,
    navigator: DestinationsNavigator
) {
    val context = LocalActivity.current ?: return
    var zipUri by remember { mutableStateOf<android.net.Uri?>(null) }

    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            zipUri?.let { navigator.navigate(FlashScreenDestination(FlashIt.FlashModules(listOf(it)))) }
            zipUri = null
        },
        onDismiss = { zipUri = null }
    )

    val intentStateValue by intentState.collectAsState()
    LaunchedEffect(intentStateValue) {
        intent?.data
            ?.takeIf { isManager && it.scheme == "content" && intent.type == "application/zip" }
            ?.also { zipUri = it }
            ?.let {
                confirmDialog.showConfirm(
                    title = context.getString(R.string.module),
                    content = context.getString(
                        R.string.module_install_prompt_with_name,
                        "\n${it.getFileName(context) ?: it.lastPathSegment ?: "Unknown"}"
                    )
                )
            }
    }
}
