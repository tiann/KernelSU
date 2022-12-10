@file:OptIn(ExperimentalMaterial3Api::class)

package me.weishu.kernelsu

import AboutDialog
import Home
import Module
import SuperUser
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import me.weishu.kernelsu.ui.theme.KernelSUTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            KernelSUTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen()
                }
            }
        }
    }
}

@Composable
fun MainTopAppBar(onMoreClick: () -> Unit) {
    TopAppBar(
        title = {
            Text(
                "KernelSU",
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        },
        actions = {
            IconButton(onClick = onMoreClick) {
                Icon(
                    imageVector = Icons.Filled.MoreVert,
                    contentDescription = "Localized description"
                )
            }
        }
    )
}

@Composable
fun MainBottomNavigation(items: List<Screen>, navController: NavHostController) {

    NavigationBar {

        val navBackStackEntry by navController.currentBackStackEntryAsState()
        val currentDestination = navBackStackEntry?.destination

        items.forEachIndexed { index, item ->
            NavigationBarItem(
                icon = {
                    Icon(
                        painter = painterResource(id = item.icon),
                        contentDescription = ""
                    )
                },
                label = { Text(text = stringResource(id = item.resourceId)) },
                selected = currentDestination?.hierarchy?.any { it.route == item.route } == true,

                onClick = {
                    navController.navigate(item.route) {
                        // Pop up to the start destination of the graph to
                        // avoid building up a large stack of destinations
                        // on the back stack as users select items
                        popUpTo(navController.graph.findStartDestination().id) {
                            saveState = true
                        }
                        // Avoid multiple copies of the same destination when
                        // reselecting the same item
                        launchSingleTop = true
                        // Restore state when reselecting a previously selected item
                        restoreState = true
                    }
                }
            )
        }
    }

}

@Composable
fun MainScreen() {

    val items = listOf(
        Screen.Home,
        Screen.SuperUser,
        Screen.Module
    )

    val navController = rememberNavController()

    var showAboutDialog by remember { mutableStateOf(false) }

    AboutDialog(openDialog = showAboutDialog, onDismiss = {
        showAboutDialog = false
    })

    Scaffold(
        topBar = {
            MainTopAppBar {
                showAboutDialog = true
            }
        },
        bottomBar = {
            MainBottomNavigation(items = items, navController = navController)
        },
        content = { innerPadding ->
            NavHost(
                navController,
                startDestination = Screen.Home.route,
                Modifier.padding(innerPadding)
            ) {
                composable(Screen.Home.route) { Home() }
                composable(Screen.SuperUser.route) { SuperUser() }
                composable(Screen.Module.route) { Module() }
            }
        }
    )

}

@Preview(showBackground = true)
@Composable
fun DefaultPreview() {
    KernelSUTheme {
        MainScreen()
    }
}

sealed class Screen(val route: String, @StringRes val resourceId: Int, val icon: Int) {
    object Home : Screen("home", R.string.home, R.drawable.ic_home)
    object SuperUser : Screen("superuser", R.string.superuser, R.drawable.ic_superuser)
    object Module : Screen("module", R.string.module, R.drawable.ic_module)
}

