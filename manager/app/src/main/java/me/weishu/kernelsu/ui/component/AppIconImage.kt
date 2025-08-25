package me.weishu.kernelsu.ui.component

import android.content.pm.PackageInfo
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.graphics.drawable.toBitmap
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.G2RoundedCornerShape

@Composable
fun AppIconImage(
    packageInfo: PackageInfo,
    label: String,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    var icon by remember(packageInfo.packageName) { mutableStateOf<ImageBitmap?>(null) }

    LaunchedEffect(packageInfo.packageName) {
        withContext(Dispatchers.IO) {
            val drawable = packageInfo.applicationInfo?.loadIcon(context.packageManager)
            val bitmap = drawable?.toBitmap()?.asImageBitmap()
            icon = bitmap
        }
    }

    icon.let { imageBitmap ->
        imageBitmap?.let {
            Image(
                bitmap = it,
                contentDescription = label,
                modifier = modifier
            )
        }
    } ?: Box(
        modifier = modifier
            .clip(G2RoundedCornerShape(12.dp))
            .background(colorScheme.secondaryContainer),
        contentAlignment = Alignment.Center
    ) {}
}
