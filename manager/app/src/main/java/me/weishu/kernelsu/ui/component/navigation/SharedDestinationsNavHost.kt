package me.weishu.kernelsu.ui.component.navigation

import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.SharedTransitionLayout
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.navigation.NavHostController
import com.ramcosta.composedestinations.DestinationsNavHost
import com.ramcosta.composedestinations.animations.NavHostAnimatedDestinationStyle
import com.ramcosta.composedestinations.navigation.DependenciesContainerBuilder
import com.ramcosta.composedestinations.rememberNavHostEngine
import com.ramcosta.composedestinations.spec.Direction
import com.ramcosta.composedestinations.spec.NavHostEngine
import com.ramcosta.composedestinations.spec.NavHostGraphSpec
import com.ramcosta.composedestinations.utils.rememberDestinationsNavigator


val LocalSharedTransitionScope = compositionLocalOf<SharedTransitionScope> { error("SharedTransitionScope not provided") }
val localPopState = compositionLocalOf { false }
val LocalAnimatedVisibilityScope = compositionLocalOf<AnimatedVisibilityScope> { error("AnimatedVisibilityScope not provided") }
val LocalRoutePopupStack = compositionLocalOf<RoutePopupStack> {
    error("RoutePopupStack not provided")
}

@Composable
fun SharedDestinationsNavHost(
    navGraph: NavHostGraphSpec,
    modifier: Modifier = Modifier,
    start: Direction = navGraph.defaultStartDirection,
    defaultTransitions: NavHostAnimatedDestinationStyle = defaultTransitions(),
    engine: NavHostEngine = rememberNavHostEngine(),
    navController: NavHostController,
    overlayContent: @Composable MiuixDestinationsNavigator.() -> Unit = {},
    dependenciesContainerBuilder: @Composable DependenciesContainerBuilder<*>.() -> Unit = {},
    manualComposableCallsBuilder: MiuixManualComposableCallsBuilder.() -> Unit = {},
) {

    SharedTransitionLayout {
        val routePopupState = rememberSaveable(saver = RoutePopupStack.Saver) {
            RoutePopupStack().apply {
                put(navGraph.startRoute.route, true)
            }
        }
        CompositionLocalProvider(
            LocalSharedTransitionScope provides this@SharedTransitionLayout,
            LocalRoutePopupStack provides routePopupState
        ) {
            DestinationsNavHost(
                modifier = modifier.fillMaxSize(),
                engine = engine,
                start = start,
                navGraph = navGraph,
                navController = navController,
                defaultTransitions = defaultTransitions,
                dependenciesContainerBuilder = dependenciesContainerBuilder,
                manualComposableCallsBuilder = { MiuixManualComposableCallsBuilder(this, routePopupState).manualComposableCallsBuilder() }
            )
            val navigator = navController.rememberDestinationsNavigator()
            MiuixDestinationsNavigator(navigator, routePopupState).overlayContent()
        }
    }
}

