package me.weishu.kernelsu.ui.component.navigation

import androidx.annotation.MainThread
import androidx.compose.runtime.Composable
import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavOptions
import androidx.navigation.Navigator
import com.ramcosta.composedestinations.navigation.DestinationsNavOptionsBuilder
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.ramcosta.composedestinations.scope.DestinationScope
import com.ramcosta.composedestinations.spec.Direction
import com.ramcosta.composedestinations.spec.RouteOrDirection

class MiuixDestinationsNavigator(
    private val destinationsNavigator: DestinationsNavigator,
    private val routePopupState: RoutePopupStack,
) {
    fun navigate(
        direction: Direction,
        builder: DestinationsNavOptionsBuilder.() -> Unit
    ) {
        routePopupState.apply {
            putLast(true)
            put(direction.route.substringBefore('/'), false)
        }
        destinationsNavigator.navigate(direction, builder)
    }

    fun navigate(
        direction: Direction,
        navOptions: NavOptions? = null,
        navigatorExtras: Navigator.Extras? = null
    ) {
        routePopupState.apply {
            putLast(true)
            put(direction.route.substringBefore('/'), false)
        }
        destinationsNavigator.navigate(direction, navOptions, navigatorExtras)
    }

    @MainThread
    fun navigateUp(): Boolean {
        return destinationsNavigator.navigateUp()
    }

    @MainThread
    fun popBackStack(): Boolean {
        if (routePopupState._keyOrder.size <= 1) return false
        routePopupState.removeLast()
        return destinationsNavigator.popBackStack()
    }

    @MainThread
    fun popBackStack(
        route: RouteOrDirection,
        inclusive: Boolean,
        saveState: Boolean = false,
    ): Boolean {
        routePopupState.remove(route.route)
        return destinationsNavigator.popBackStack(route, inclusive, saveState)
    }

    @MainThread
    fun clearBackStack(route: RouteOrDirection): Boolean {
        routePopupState.clear()
        return destinationsNavigator.clearBackStack(route)
    }

    fun getBackStackEntry(
        route: RouteOrDirection
    ): NavBackStackEntry? {
        return destinationsNavigator.getBackStackEntry(route)
    }


}

@Composable
fun <T> DestinationScope<T>.miuixDestinationsNavigator(): MiuixDestinationsNavigator =
    MiuixDestinationsNavigator(destinationsNavigator, LocalRoutePopupStack.current)
