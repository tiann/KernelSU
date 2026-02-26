package me.weishu.kernelsu.ui.component

import android.content.pm.PackageInfo
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import coil3.compose.AsyncImage
import com.kyant.capsule.ContinuousRoundedRectangle
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun AppIconImage(
    packageInfo: PackageInfo,
    label: String,
    modifier: Modifier = Modifier
) {
    var isSuccess by remember { mutableStateOf(false) }

    Box(modifier = modifier) {
        if (!isSuccess) {
            Box(
                modifier = Modifier
                    .matchParentSize()
                    .clip(ContinuousRoundedRectangle(12.dp))
                    .background(colorScheme.secondaryContainer),
                contentAlignment = Alignment.Center
            ) {}
        }
        AsyncImage(
            model = packageInfo,
            contentDescription = label,
            modifier = Modifier.matchParentSize(),
            onSuccess = { isSuccess = true },
            onLoading = { isSuccess = false },
            onError = { isSuccess = false }
        )
    }
}
