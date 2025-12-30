package me.weishu.kernelsu.ui.component

import androidx.navigation.NavOptions
import androidx.navigation.navOptions

val noAnimationNavOptions = navOptions{
    anim {
        enter = 0
        exit = 0
        popEnter = 0
        popExit = 0
    }
    launchSingleTop = true
}

