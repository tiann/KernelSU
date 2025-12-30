package me.weishu.kernelsu.ui.component.navigation

import android.util.Log
import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.SharedTransitionLayout
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.Modifier.Companion
import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavGraph
import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.Navigator
import androidx.navigation.compose.ComposeNavigator
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.createGraph
import androidx.navigation.get
import com.ramcosta.composedestinations.DestinationsNavHost
import com.ramcosta.composedestinations.animations.NavHostAnimatedDestinationStyle
import com.ramcosta.composedestinations.generated.NavGraphs
import com.ramcosta.composedestinations.manualcomposablecalls.ManualComposableCallsBuilder
import com.ramcosta.composedestinations.navigation.DependenciesContainerBuilder
import com.ramcosta.composedestinations.rememberNavHostEngine
import com.ramcosta.composedestinations.spec.Direction
import com.ramcosta.composedestinations.spec.NavHostEngine
import com.ramcosta.composedestinations.spec.NavHostGraphSpec
import com.ramcosta.composedestinations.utils.currentDestinationFlow
import com.ramcosta.composedestinations.utils.isRouteOnBackStack
import com.ramcosta.composedestinations.utils.isRouteOnBackStackAsState
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.distinctUntilChanged
import top.yukonga.miuix.kmp.theme.MiuixTheme
import kotlin.math.log
import kotlin.reflect.KClass
import kotlin.reflect.KType


val LocalSharedTransitionScope = compositionLocalOf<SharedTransitionScope?> { null }
val localPopState = compositionLocalOf { false }
val LocalAnimatedVisibilityScope =  compositionLocalOf<AnimatedVisibilityScope?> { null }
val routePopupState = RoutePopupStack()
@Composable
fun SharedDestinationsNavHost(
    navGraph: NavHostGraphSpec,
    modifier: Modifier = Modifier,
    start: Direction = navGraph.defaultStartDirection,
    defaultTransitions: NavHostAnimatedDestinationStyle = defaultTransitions(),
    engine: NavHostEngine = rememberNavHostEngine(),
    navController: NavHostController = engine.rememberNavController(),
    dependenciesContainerBuilder: @Composable DependenciesContainerBuilder<*>.() -> Unit = {},
    manualComposableCallsBuilder: ManualComposableCallsBuilder.() -> Unit = {},
){

    SharedTransitionLayout{
        CompositionLocalProvider(
            LocalSharedTransitionScope provides this@SharedTransitionLayout,
        ) {
            routePopupState.put( NavGraphs.root.startRoute.route, true)
            DestinationsNavHost(
                modifier = modifier,
                engine = engine,
                start = start,
                navGraph = NavGraphs.root,
                navController = navController,
                defaultTransitions = defaultTransitions,
                dependenciesContainerBuilder = dependenciesContainerBuilder,
                manualComposableCallsBuilder = manualComposableCallsBuilder
            )
        }
    }
}


object MiuixNavHostDefaults {
    const val TRANSITION_DURATION = 500
    const val SHARETRANSITION_DURATION = 550

    val NavAnimationEasing = NavTransitionEasing(0.8f, 0.95f)
}