package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.isTraversalGroup
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.kyant.capsule.ContinuousRoundedRectangle
import com.kyant.capsule.continuities.G1Continuity
import top.yukonga.miuix.kmp.basic.CardColors
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.theme.LocalContentColor
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.SinkFeedback
import top.yukonga.miuix.kmp.utils.TiltFeedback
import top.yukonga.miuix.kmp.utils.pressable

@Composable
fun SharedTransitionCard(
    modifier: Modifier = Modifier,
    cornerRadius: Dp = CardDefaults.CornerRadius,
    insideMargin: PaddingValues = CardDefaults.InsideMargin,
    colors: CardColors = CardDefaults.defaultColors(),
    pressFeedbackType: PressFeedbackType = PressFeedbackType.None,
    showIndication: Boolean? = false,
    onClick: (() -> Unit)? = null,
    onLongPress: (() -> Unit)? = null,
    content: @Composable ColumnScope.() -> Unit,
) {
    val interactionSource = remember { MutableInteractionSource() }
    val currentOnClick by rememberUpdatedState(onClick)
    val currentOnLongPress by rememberUpdatedState(onLongPress)

    val pressFeedback = remember(pressFeedbackType) {
        when (pressFeedbackType) {
            PressFeedbackType.None -> null
            PressFeedbackType.Sink -> SinkFeedback()
            PressFeedbackType.Tilt -> TiltFeedback()
        }
    }

    val usedInteractionSource by remember(pressFeedback) {
        derivedStateOf { if (pressFeedback != null) interactionSource else null }
    }
    val indicationLocal = LocalIndication.current
    val indicationToUse = remember(showIndication, indicationLocal) {
        if (showIndication == true) indicationLocal else null
    }

    val shape = remember(cornerRadius) { ContinuousRoundedRectangle(cornerRadius) }
    val clipShape = remember(cornerRadius) { ContinuousRoundedRectangle(cornerRadius, G1Continuity) }

    CompositionLocalProvider(
        LocalContentColor provides colors.contentColor,
    ) {
        Box(
            modifier = Modifier
                .padding(bottom = 12.dp)
                .semantics(mergeDescendants = false) {
                    isTraversalGroup = true
                }
                .then(modifier)
                .pressable(
                    interactionSource = usedInteractionSource,
                    indication = pressFeedback,
                    delay = null
                )
                .background(color = colors.color, shape = shape)
                .clip(clipShape),
            contentAlignment = Alignment.TopCenter,
            propagateMinConstraints = true,
        ) {
            Column(
                modifier = Modifier
                    .background(color = colors.color)
                    .padding(insideMargin)
                    .combinedClickable(
                        interactionSource = interactionSource,
                        indication = indicationToUse,
                        onClick = { currentOnClick?.invoke() },
                        onLongClick = currentOnLongPress
                    )
                ,
                content = content
            )
        }
    }
}