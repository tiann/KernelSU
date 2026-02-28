package me.weishu.kernelsu.ui.component

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.UserHandle
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
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
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.dp
import androidx.core.graphics.createBitmap
import androidx.core.graphics.drawable.toDrawable
import com.kyant.capsule.ContinuousRoundedRectangle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun AppIconImage(
    modifier: Modifier = Modifier, applicationInfo: ApplicationInfo, label: String? = null
) {
    val density = LocalDensity.current
    val context = LocalContext.current
    val resources = LocalResources.current

    BoxWithConstraints(modifier = modifier) {
        val targetSizePx = remember(constraints, density) {
            if (constraints.maxWidth != Constraints.Infinity) {
                constraints.maxWidth
            } else {
                with(density) { 48.dp.roundToPx() }
            }
        }

        if (targetSizePx == 0) {
            PlaceHolderBox(Modifier.fillMaxSize())
            return@BoxWithConstraints
        }

        var appIcon by remember { mutableStateOf<Drawable?>(null) }

        LaunchedEffect(applicationInfo, targetSizePx) {
            val loadedIcon = withContext(Dispatchers.IO) {
                val pm = context.packageManager
                var finalDrawable: Drawable? = null

                try {
                    val appRes = pm.getResourcesForApplication(applicationInfo)
                    val iconId = applicationInfo.icon

                    if (iconId != 0) {
                        // Decode only the bounds, do not load pixels
                        val options = BitmapFactory.Options().apply {
                            inJustDecodeBounds = true
                        }
                        BitmapFactory.decodeResource(appRes, iconId, options)

                        if (options.outWidth > targetSizePx * 6 || options.outHeight > targetSizePx * 6) {
                            options.inSampleSize = calculateInSampleSize(options, targetSizePx, targetSizePx)
                            options.inJustDecodeBounds = false

                            val scaledBitmap = BitmapFactory.decodeResource(appRes, iconId, options)
                            if (scaledBitmap != null) {
                                finalDrawable = scaledBitmap.toDrawable(resources)
                            }
                        } else {
                            finalDrawable = null
                        }
                    }
                } catch (_: Exception) {
                    finalDrawable = null
                }

                if (finalDrawable == null) {
                    finalDrawable = applicationInfo.loadUnbadgedIcon(pm)
                }

                // Add system badges
                val handle = UserHandle.getUserHandleForUid(applicationInfo.uid)
                val badgedDrawable = pm.getUserBadgedIcon(finalDrawable, handle)

                if (badgedDrawable is BitmapDrawable && (badgedDrawable.intrinsicWidth > targetSizePx * 2 || badgedDrawable.intrinsicHeight > targetSizePx * 2)) {

                    val scaledBitmap = createBitmap(targetSizePx, targetSizePx)
                    val canvas = Canvas(scaledBitmap)
                    badgedDrawable.setBounds(0, 0, canvas.width, canvas.height)
                    badgedDrawable.draw(canvas)

                    scaledBitmap.toDrawable(resources)
                } else {
                    badgedDrawable
                }
            }
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

private fun calculateInSampleSize(options: BitmapFactory.Options, reqWidth: Int, reqHeight: Int): Int {
    val height: Int = options.outHeight
    val width: Int = options.outWidth
    var inSampleSize = 1

    if (height > reqHeight || width > reqWidth) {
        // Calculate the largest inSampleSize value that is a power of 2 and keeps both
        // height and width larger than or equal to the requested height and width.
        while ((height / (inSampleSize * 2)) >= reqHeight && (width / (inSampleSize * 2)) >= reqWidth) {
            inSampleSize *= 2
        }
    }

    return inSampleSize
}