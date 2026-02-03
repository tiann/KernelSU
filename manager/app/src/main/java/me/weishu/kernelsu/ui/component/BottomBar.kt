package me.weishu.kernelsu.ui.component

import androidx.annotation.StringRes
import androidx.compose.animation.core.EaseInOut
import androidx.compose.animation.core.tween
import androidx.compose.foundation.gestures.animateScrollBy
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Cottage
import androidx.compose.material.icons.rounded.Extension
import androidx.compose.material.icons.rounded.Security
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.hazeEffect
import kotlinx.coroutines.Job
import kotlinx.coroutines.job
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalPagerState
import me.weishu.kernelsu.ui.util.rootAvailable
import top.yukonga.miuix.kmp.basic.NavigationBar
import top.yukonga.miuix.kmp.basic.NavigationItem


@Composable
fun BottomBar(
    hazeState: HazeState,
    hazeStyle: HazeStyle
) {
    val isManager = Natives.isManager
    val fullFeatured = isManager && !Natives.requireNewKernel() && rootAvailable()

    val pageState = LocalPagerState.current
    val coroutineScope = rememberCoroutineScope()
    var selectedPage by remember { mutableIntStateOf(pageState.currentPage) }
    var isNavigating by remember { mutableStateOf(false) }

    LaunchedEffect(pageState.currentPage) {
        if (!isNavigating) {
            selectedPage = pageState.currentPage
        }
    }

    if (!fullFeatured) return

    val item = BottomBarDestination.entries.map { destination ->
        NavigationItem(
            label = stringResource(destination.label),
            icon = destination.icon,
        )
    }

    val currentNavJob = remember { mutableStateOf<Job?>(null) }

    NavigationBar(
        modifier = Modifier
            .hazeEffect(hazeState) {
                style = hazeStyle
                blurRadius = 30.dp
                noiseFactor = 0f
            },
        color = Color.Transparent,
        items = item,
        selected = selectedPage,
        onClick = { targetIndex ->
            if (targetIndex == selectedPage) return@NavigationBar
            currentNavJob.value?.cancel()
            selectedPage = targetIndex
            isNavigating = true
            val distance = kotlin.math.abs(targetIndex - pageState.currentPage).coerceAtLeast(2)
            val duration = 100 * distance + 100
            val layoutInfo = pageState.layoutInfo
            val pageSize = layoutInfo.pageSize + layoutInfo.pageSpacing
            val currentDistanceInPages = targetIndex - pageState.currentPage - pageState.currentPageOffsetFraction
            val scrollPixels = currentDistanceInPages * pageSize
            currentNavJob.value = coroutineScope.launch {
                val myJob = coroutineContext.job
                try {
                    pageState.animateScrollBy(value = scrollPixels, animationSpec = tween(easing = EaseInOut, durationMillis = duration))
                } finally {
                    if (currentNavJob.value == myJob) {
                        isNavigating = false
                        if (pageState.currentPage != targetIndex) {
                            selectedPage = pageState.currentPage
                        }
                    }
                }
            }
        }
    )
}

enum class BottomBarDestination(
    @get:StringRes val label: Int,
    val icon: ImageVector,
) {
    Home(R.string.home, Icons.Rounded.Cottage),
    SuperUser(R.string.superuser, Icons.Rounded.Security),
    Module(R.string.module, Icons.Rounded.Extension),
    Setting(R.string.settings, Icons.Rounded.Settings)
}
