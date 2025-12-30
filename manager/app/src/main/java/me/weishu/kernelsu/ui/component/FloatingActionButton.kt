package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousCapsule
import top.yukonga.miuix.kmp.basic.Surface
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun FloatingActionButton(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    contentModifier: Modifier,
    shape: Shape = ContinuousCapsule,
    containerColor: Color = MiuixTheme.colorScheme.primary,
    shadowElevation: Dp = 4.dp,
    minWidth: Dp = 60.dp,
    minHeight: Dp = 60.dp,
    content: @Composable () -> Unit,
) {
    val currentOnClick by rememberUpdatedState(onClick)

    Surface(
        onClick = currentOnClick,
        modifier = modifier
            .defaultMinSize(
                minWidth = minWidth,
                minHeight = minHeight,
            )
            .semantics { role = Role.Button },
        shape = shape,
        shadowElevation = shadowElevation
    ) {
        Box(
            modifier = contentModifier.background(containerColor),
            contentAlignment = Alignment.Center,
        ) {
            content()
        }
    }
}