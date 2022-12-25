package me.weishu.kernelsu.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.ExperimentalAnimationApi
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.NavHostController
import com.google.accompanist.navigation.animation.rememberAnimatedNavController
import com.ramcosta.composedestinations.DestinationsNavHost
import me.weishu.kernelsu.ui.screen.BottomBarDestination
import me.weishu.kernelsu.ui.screen.NavGraphs
import me.weishu.kernelsu.ui.screen.appCurrentDestinationAsState
import me.weishu.kernelsu.ui.screen.destinations.Destination
import me.weishu.kernelsu.ui.screen.startAppDestination
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.util.LocalSnackbarHost

class MainActivity : ComponentActivity() {

    @OptIn(ExperimentalAnimationApi::class, ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            KernelSUTheme {
                val navController = rememberAnimatedNavController()
                val snackbarHostState = remember { SnackbarHostState() }
                Scaffold(
                    bottomBar = { BottomBar(navController) },
                    snackbarHost = { SnackbarHost(snackbarHostState) }
                ) { innerPadding ->
                    CompositionLocalProvider(LocalSnackbarHost provides snackbarHostState) {
                        DestinationsNavHost(
                            modifier = Modifier.padding(innerPadding),
                            navGraph = NavGraphs.root,
                            navController = navController
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun BottomBar(navController: NavHostController) {
    val currentDestination: Destination = navController.appCurrentDestinationAsState().value
        ?: NavGraphs.root.startAppDestination
    var topDestination by rememberSaveable { mutableStateOf(currentDestination.route) }
    LaunchedEffect(currentDestination) {
        val queue = navController.backQueue
        if (queue.size == 2) topDestination = queue[1].destination.route!!
        else if (queue.size > 2) topDestination = queue[2].destination.route!!
    }

    NavigationBar(tonalElevation = 8.dp) {
        BottomBarDestination.values().forEach { destination ->
            NavigationBarItem(
                selected = topDestination == destination.direction.route,
                onClick = {
                    navController.navigate(destination.direction.route) {
                        popUpTo(navController.graph.findStartDestination().id) {
                            saveState = true
                        }
                        launchSingleTop = true
                        restoreState = true
                    }
                },
                icon = {
                    if (topDestination == destination.direction.route) Icon(destination.iconSelected, stringResource(destination.label))
                    else Icon(destination.iconNotSelected, stringResource(destination.label))
                },
                label = { Text(stringResource(destination.label)) },
                alwaysShowLabel = false
            )
        }
    }
}
