package me.weishu.kernelsu.ui.component.navigation

import android.util.Log
import androidx.navigation.NavOptions
import androidx.navigation.Navigator
import com.ramcosta.composedestinations.generated.NavGraphs
import com.ramcosta.composedestinations.navigation.DestinationsNavOptionsBuilder
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.ramcosta.composedestinations.result.ResultBackNavigator
import com.ramcosta.composedestinations.spec.Direction


fun ResultBackNavigator<Boolean>.navigateBackEx(result: Boolean){
    routePopupState.removeLast()
    navigateBack(result)
}
fun DestinationsNavigator.popBackStackEx(){
    routePopupState.removeLast()
    popBackStack()
}
fun DestinationsNavigator.navigateEx(
    direction: Direction,
    builder: DestinationsNavOptionsBuilder.() -> Unit
){
    routePopupState.apply {
        putLast( true)
        put(direction.route.substringBefore('/'),false)
    }
    navigate(direction,builder)
}
fun DestinationsNavigator.navigateEx(
    direction: Direction,
    navOptions: NavOptions? = null,
    navigatorExtras: Navigator.Extras? = null
){
    routePopupState.apply {
        putLast( true)
        put(direction.route.substringBefore('/'),false)
    }
    navigate(direction,navOptions,navigatorExtras)
}
