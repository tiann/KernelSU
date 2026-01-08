package me.weishu.kernelsu.ui.component.navigation

import androidx.compose.runtime.Composable
import com.ramcosta.composedestinations.navargs.DestinationsNavType
import com.ramcosta.composedestinations.result.ResultBackNavigator
import com.ramcosta.composedestinations.scope.DestinationScopeWithNoDependencies
import com.ramcosta.composedestinations.scope.resultBackNavigator

@Composable
fun <R> DestinationScopeWithNoDependencies<*>.resultBackMiuixNavigator(
    resultNavType: DestinationsNavType<in R>
): ResultBackMiuixNavigator<R> = ResultBackMiuixNavigator(
    resultBackNavigator(resultNavType),
    LocalRoutePopupStack.current
)

@Composable
fun <R> resultBackMiuixNavigator(
    resultBackNavigator: ResultBackNavigator<R>,
): ResultBackMiuixNavigator<R> = ResultBackMiuixNavigator(
    resultBackNavigator,
    LocalRoutePopupStack.current
)

class ResultBackMiuixNavigator<R>(
    val resultBackNavigator: ResultBackNavigator<R>,
    val routePopupStack: RoutePopupStack
) {

    fun navigateBack(result: R) {
        if (routePopupStack._keyOrder.size <= 1) return
        routePopupStack.removeLast()
        resultBackNavigator.navigateBack(result)
    }

    fun setResult(result: R) {
        resultBackNavigator.setResult(result)
    }

    fun navigateBack() {
        if (routePopupStack._keyOrder.size <= 1) return
        routePopupStack.removeLast()
        resultBackNavigator.navigateBack()
    }

}

