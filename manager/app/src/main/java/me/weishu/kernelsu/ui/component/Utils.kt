package me.weishu.kernelsu.ui.component

import android.app.Activity
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

var screenCornerRadius = (-1).dp

@Composable
fun getCornerRadiusTop(): Dp {
    if (screenCornerRadius == -1.dp){
        val mResource = LocalResources.current
        val resourceId = mResource.getIdentifier("rounded_corner_radius_top", "dimen", "android")
        screenCornerRadius = if (resourceId > 0) {
            (mResource.getDimension(resourceId) / mResource.displayMetrics.density).toInt().coerceAtLeast(0).dp
        } else {
            0.dp
        }
    }
    return screenCornerRadius
}