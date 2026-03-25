package me.weishu.kernelsu.ui.component

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.ui.util.AppIconCache
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun AppIconImage(
    modifier: Modifier = Modifier,
    applicationInfo: ApplicationInfo,
    label: String,
) {
    val density = LocalDensity.current
    val context = LocalContext.current
    val targetSizePx = with(density) { 48.dp.roundToPx() }

    val cachedBitmap = remember(applicationInfo) { AppIconCache.getFromCache(applicationInfo) }

    Box(modifier = modifier) {
        var appBitmap by remember(applicationInfo) { mutableStateOf(cachedBitmap) }

        if (cachedBitmap == null) {
            LaunchedEffect(applicationInfo) {
                appBitmap = AppIconCache.loadIcon(context, applicationInfo, targetSizePx)
            }
        }

        if (cachedBitmap != null) {
            val imageBitmap = remember(appBitmap) { appBitmap!!.asImageBitmap() }
            Image(
                bitmap = imageBitmap,
                contentDescription = label,
                modifier = Modifier.fillMaxSize()
            )
        } else {
            Crossfade(
                targetState = appBitmap,
                animationSpec = tween(durationMillis = 150),
                label = "IconFade"
            ) { icon ->
                if (icon == null) {
                    PlaceHolderBox(Modifier.fillMaxSize())
                } else {
                    val imageBitmap = remember(icon) { icon.asImageBitmap() }
                    Image(
                        bitmap = imageBitmap,
                        contentDescription = label,
                        modifier = Modifier.fillMaxSize()
                    )
                }
            }
        }
    }
}

@Composable
fun AppIconImage(
    modifier: Modifier = Modifier,
    packageInfo: PackageInfo,
    label: String,
) {
    val appInfo = packageInfo.applicationInfo
    if (appInfo == null) {
        PlaceHolderBox(modifier)
        return
    }
    AppIconImage(modifier, appInfo, label)
}

@Composable
private fun PlaceHolderBox(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(colorScheme.secondaryContainer)
    )
}