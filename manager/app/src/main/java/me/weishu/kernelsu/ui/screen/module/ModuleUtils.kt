package me.weishu.kernelsu.ui.screen.module

import android.content.Context
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.EaseInOutCubic
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.SubcomposeLayout
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.TextUnit
import me.weishu.kernelsu.ui.util.module.Shortcut
import kotlin.math.roundToInt

enum class ShortcutType {
    Action,
    WebUI
}

fun hasModuleShortcut(context: Context, moduleId: String, type: ShortcutType): Boolean {
    return when (type) {
        ShortcutType.Action -> Shortcut.hasModuleActionShortcut(context, moduleId)
        ShortcutType.WebUI -> Shortcut.hasModuleWebUiShortcut(context, moduleId)
    }
}

fun deleteModuleShortcut(context: Context, moduleId: String, type: ShortcutType) {
    when (type) {
        ShortcutType.Action -> Shortcut.deleteModuleActionShortcut(context, moduleId)
        ShortcutType.WebUI -> Shortcut.deleteModuleWebUiShortcut(context, moduleId)
    }
}

fun createModuleShortcut(
    context: Context,
    moduleId: String,
    name: String,
    iconUri: String?,
    type: ShortcutType
) {
    when (type) {
        ShortcutType.Action -> {
            Shortcut.createModuleActionShortcut(
                context = context,
                moduleId = moduleId,
                name = name,
                iconUri = iconUri
            )
        }

        ShortcutType.WebUI -> {
            Shortcut.createModuleWebUiShortcut(
                context = context,
                moduleId = moduleId,
                name = name,
                iconUri = iconUri
            )
        }
    }
}

private enum class DescriptionSlot {
    Collapsed,
    Expanded
}

@Composable
fun ExpandableDescriptionText(
    text: String,
    expanded: Boolean,
    maxLinesLimit: Int,
    modifier: Modifier = Modifier,
    color: Color = Color.Unspecified,
    style: TextStyle = LocalTextStyle.current,
    fontSize: TextUnit = TextUnit.Unspecified,
    textDecoration: TextDecoration? = null,
) {
    val progress = remember(text) { Animatable(if (expanded) 1f else 0f) }

    LaunchedEffect(expanded) {
        val target = if (expanded) 1f else 0f
        if (progress.targetValue != target) {
            val spec = if (expanded) {
                tween<Float>(durationMillis = 280, easing = FastOutSlowInEasing)
            } else {
                tween<Float>(durationMillis = 320, easing = EaseInOutCubic)
            }
            progress.animateTo(target, animationSpec = spec)
        }
    }

    SubcomposeLayout(
        modifier = modifier.clipToBounds()
    ) { constraints ->
        val collapsedPlaceable = subcompose(DescriptionSlot.Collapsed) {
            Text(
                text = text,
                color = color,
                style = style,
                fontSize = fontSize,
                textDecoration = textDecoration,
                maxLines = maxLinesLimit,
                overflow = TextOverflow.Ellipsis
            )
        }.first().measure(constraints)

        val expandedPlaceable = subcompose(DescriptionSlot.Expanded) {
            Text(
                text = text,
                color = color,
                style = style,
                fontSize = fontSize,
                textDecoration = textDecoration,
                maxLines = Int.MAX_VALUE,
                overflow = TextOverflow.Clip
            )
        }.first().measure(constraints)

        val collapsedHeight = collapsedPlaceable.height
        val expandedHeight = expandedPlaceable.height
        val canExpand = expandedHeight > collapsedHeight

        if (!canExpand) {
            val width = collapsedPlaceable.width.coerceIn(constraints.minWidth, constraints.maxWidth)
            val height = collapsedHeight.coerceIn(constraints.minHeight, constraints.maxHeight)
            layout(width, height) {
                collapsedPlaceable.place(0, 0)
            }
        } else {
            val currentProgress = progress.value
            val currentHeight = (collapsedHeight + (expandedHeight - collapsedHeight) * currentProgress)
                .roundToInt()
                .coerceIn(constraints.minHeight, constraints.maxHeight)
            val placeableToUse = if (currentProgress == 0f) collapsedPlaceable else expandedPlaceable
            val width = placeableToUse.width.coerceIn(constraints.minWidth, constraints.maxWidth)

            layout(width, currentHeight) {
                placeableToUse.place(0, 0)
            }
        }
    }
}
