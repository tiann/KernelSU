package me.weishu.kernelsu.ui.component

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.graphics.drawable.Drawable
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.graphics.withSave
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousRoundedRectangle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ui.util.AppIconCache
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun AppIconImage(
    modifier: Modifier = Modifier,
    applicationInfo: ApplicationInfo,
    label: String? = null
) {
    val density = LocalDensity.current
    val context = LocalContext.current
    val targetSizePx = with(density) { 48.dp.roundToPx() }

    Box(modifier = modifier) {
        var appIcon by remember { mutableStateOf<Drawable?>(null) }

        LaunchedEffect(applicationInfo) {
            val loadedIcon = AppIconCache.loadIcon(context, applicationInfo, targetSizePx)
            appIcon = loadedIcon
        }

        val appLabel by produceState(initialValue = label, key1 = applicationInfo) {
            if (label != null) {
                value = label
            } else {
                withContext(Dispatchers.IO) {
                    val pm = context.packageManager
                    val appLabel = pm.getApplicationLabel(applicationInfo).toString()
                    value = appLabel
                }
            }
        }

        Crossfade(
            targetState = appIcon, animationSpec = tween(durationMillis = 150), label = "IconFade"
        ) { icon ->
            if (icon == null) {
                PlaceHolderBox(Modifier.fillMaxSize())
            } else {
                val painter = remember(icon) { DrawablePainter(icon) }
                Image(
                    painter = painter, contentDescription = appLabel, modifier = Modifier.fillMaxSize()
                )
            }
        }
    }
}

@Composable
fun AppIconImage(modifier: Modifier = Modifier, packageInfo: PackageInfo, label: String? = null) {
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
            .clip(ContinuousRoundedRectangle(12.dp))
            .background(colorScheme.secondaryContainer)
    )
}

class DrawablePainter(private val drawable: Drawable) : Painter() {

    override val intrinsicSize: Size
        get() = Size(
            width = drawable.intrinsicWidth.toFloat(), height = drawable.intrinsicHeight.toFloat()
        )

    init {
        drawable.setBounds(0, 0, drawable.intrinsicWidth, drawable.intrinsicHeight)
    }

    override fun DrawScope.onDraw() {
        drawContext.canvas.withSave {
            drawable.setBounds(0, 0, size.width.toInt(), size.height.toInt())
            drawable.draw(drawContext.canvas.nativeCanvas)
        }
    }
}
