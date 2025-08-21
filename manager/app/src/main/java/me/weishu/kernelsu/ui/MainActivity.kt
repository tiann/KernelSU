package me.weishu.kernelsu.ui

import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
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
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.navigation.NavBackStackEntry
import androidx.navigation.compose.rememberNavController
import com.ramcosta.composedestinations.DestinationsNavHost
import com.ramcosta.composedestinations.animations.NavHostAnimatedDestinationStyle
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.NavGraphs
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeSource
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.component.BottomBar
import me.weishu.kernelsu.ui.screen.HomePager
import me.weishu.kernelsu.ui.screen.ModulePager
import me.weishu.kernelsu.ui.screen.SuperUserPager
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.util.install
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.theme.MiuixTheme

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {

        // Enable edge to edge
        enableEdgeToEdge()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isNavigationBarContrastEnforced = false
        }

        super.onCreate(savedInstanceState)

        val isManager = Natives.becomeManager(ksuApp.packageName)
        if (isManager) install()

        setContent {
            KernelSUTheme {
                val navController = rememberNavController()

                Scaffold { innerPadding ->
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
}


val LocalPagerState = compositionLocalOf<PagerState> { error("No pager state") }
val LocalHandlePageChange = compositionLocalOf<(Int) -> Unit> { error("No handle page change") }

@Composable
@Destination<RootGraph>(start = true)
fun MainScreen(navController: DestinationsNavigator) {
    val activity = LocalActivity.current
    val coroutineScope = rememberCoroutineScope()
    val pagerState = rememberPagerState(initialPage = 0, pageCount = { 3 })
    val hazeState = remember { HazeState() }
    val hazeStyle = HazeStyle(
        backgroundColor = MiuixTheme.colorScheme.background,
        tint = HazeTint(MiuixTheme.colorScheme.background.copy(0.8f))
    )
    val handlePageChange: (Int) -> Unit = remember(pagerState, coroutineScope) {
        { page ->
            coroutineScope.launch { pagerState.animateScrollToPage(page) }
        }
    }

    BackHandler {
        if (pagerState.currentPage != 0) {
            coroutineScope.launch {
                pagerState.animateScrollToPage(0)
            }
        } else {
            activity?.finishAndRemoveTask()
        }
    }

    CompositionLocalProvider(
        LocalPagerState provides pagerState,
        LocalHandlePageChange provides handlePageChange
    ) {
        Scaffold(
            bottomBar = {
                BottomBar(hazeState, hazeStyle)
            },
        ) { innerPadding ->
            HorizontalPager(
                modifier = Modifier.hazeSource(state = hazeState),
                state = pagerState,
                beyondViewportPageCount = 1,
                userScrollEnabled = false
            ) {
                when (it) {
                    0 -> HomePager(pagerState, navController, innerPadding.calculateBottomPadding())
                    1 -> SuperUserPager(navController, innerPadding.calculateBottomPadding())
                    2 -> ModulePager(navController, innerPadding.calculateBottomPadding())
                }
            }
        }
    }
}
