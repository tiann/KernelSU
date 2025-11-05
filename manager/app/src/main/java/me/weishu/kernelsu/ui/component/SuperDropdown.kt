package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.Image
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.BasicComponentColors
import top.yukonga.miuix.kmp.basic.BasicComponentDefaults
import top.yukonga.miuix.kmp.basic.ListPopup
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.extra.DropDownMode
import top.yukonga.miuix.kmp.extra.DropdownColors
import top.yukonga.miuix.kmp.extra.DropdownDefaults
import top.yukonga.miuix.kmp.extra.DropdownImpl
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.basic.ArrowUpDownIntegrated
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun SuperDropdown(
    items: List<String>,
    selectedIndex: Int,
    title: String,
    titleColor: BasicComponentColors = BasicComponentDefaults.titleColor(),
    summary: String? = null,
    summaryColor: BasicComponentColors = BasicComponentDefaults.summaryColor(),
    leftAction: @Composable (() -> Unit)? = null,
    dropdownColors: DropdownColors = DropdownDefaults.dropdownColors(),
    mode: DropDownMode = DropDownMode.Normal,
    modifier: Modifier = Modifier,
    insideMargin: PaddingValues = BasicComponentDefaults.InsideMargin,
    maxHeight: Dp? = null,
    enabled: Boolean = true,
    showValue: Boolean = true,
    onClick: (() -> Unit)? = null,
    onSelectedIndexChange: ((Int) -> Unit)?,
) {
    val currentOnClick by rememberUpdatedState(onClick)
    val currentOnSelectedIndexChange by rememberUpdatedState(onSelectedIndexChange)

    val interactionSource = remember { MutableInteractionSource() }
    val isDropdownExpanded = remember { mutableStateOf(false) }
    val hapticFeedback = LocalHapticFeedback.current

    val itemsNotEmpty = items.isNotEmpty()
    val actualEnabled = enabled && itemsNotEmpty

    val onSurfaceVariantActionsColor = MiuixTheme.colorScheme.onSurfaceVariantActions
    val disabledOnSecondaryVariantColor = MiuixTheme.colorScheme.disabledOnSecondaryVariant

    val actionColor = remember(actualEnabled, onSurfaceVariantActionsColor, disabledOnSecondaryVariantColor) {
        if (actualEnabled) onSurfaceVariantActionsColor
        else disabledOnSecondaryVariantColor
    }

    var alignLeft by rememberSaveable { mutableStateOf(true) }

    val basicComponentModifier = remember(modifier, actualEnabled) {
        modifier
            .pointerInput(actualEnabled) {
                if (!actualEnabled) return@pointerInput
                awaitPointerEventScope {
                    while (true) {
                        val event = awaitPointerEvent()
                        if (event.type != PointerEventType.Move) {
                            val eventChange = event.changes.first()
                            if (eventChange.pressed) {
                                alignLeft = eventChange.position.x < (size.width / 2)
                            }
                        }
                    }
                }
            }
    }

    val rememberPopup: @Composable () -> Unit =
        remember(
            itemsNotEmpty, isDropdownExpanded, mode, alignLeft, maxHeight,
            items, selectedIndex, dropdownColors, hapticFeedback, currentOnSelectedIndexChange
        ) {
            @Composable {
                if (itemsNotEmpty) {
                    ListPopup(
                        show = isDropdownExpanded,
                        alignment = if ((mode == DropDownMode.AlwaysOnRight || !alignLeft))
                            PopupPositionProvider.Align.Right
                        else
                            PopupPositionProvider.Align.Left,
                        onDismissRequest = {
                            isDropdownExpanded.value = false
                        },
                        maxHeight = maxHeight
                    ) {
                        ListPopupColumn {
                            items.forEachIndexed { index, string ->
                                DropdownImpl(
                                    text = string,
                                    optionSize = items.size,
                                    isSelected = selectedIndex == index,
                                    dropdownColors = dropdownColors,
                                    onSelectedIndexChange = { selectedIdx ->
                                        hapticFeedback.performHapticFeedback(HapticFeedbackType.Confirm)
                                        currentOnSelectedIndexChange?.invoke(selectedIdx)
                                        isDropdownExpanded.value = false
                                    },
                                    index = index
                                )
                            }
                        }
                    }
                }
            }
        }

    val rememberedRightActions: @Composable RowScope.() -> Unit =
        remember(showValue, itemsNotEmpty, items, selectedIndex, actionColor) {
            @Composable {
                if (showValue && itemsNotEmpty) {
                    val rightTextModifier = remember { Modifier.widthIn(max = 130.dp) }
                    Text(
                        modifier = rightTextModifier,
                        text = items[selectedIndex],
                        fontSize = MiuixTheme.textStyles.body2.fontSize,
                        color = actionColor,
                        textAlign = TextAlign.End,
                        overflow = TextOverflow.Ellipsis,
                        maxLines = 2
                    )
                }
                val imageColorFilter = remember(actionColor) { ColorFilter.tint(actionColor) }
                val arrowImageModifier = remember {
                    Modifier
                        .padding(start = 8.dp)
                        .size(10.dp, 16.dp)
                        .align(Alignment.CenterVertically)
                }
                Image(
                    modifier = arrowImageModifier,
                    imageVector = MiuixIcons.Basic.ArrowUpDownIntegrated,
                    colorFilter = imageColorFilter,
                    contentDescription = null
                )
            }
        }

    val rememberedOnClick: () -> Unit = remember(actualEnabled, currentOnClick, isDropdownExpanded, hapticFeedback) {
        {
            if (actualEnabled) {
                currentOnClick?.invoke()
                isDropdownExpanded.value = !isDropdownExpanded.value
                if (isDropdownExpanded.value) {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.ContextClick)
                }
            }
        }
    }

    BasicComponent(
        modifier = basicComponentModifier,
        interactionSource = interactionSource,
        insideMargin = insideMargin,
        title = title,
        titleColor = titleColor,
        summary = summary,
        summaryColor = summaryColor,
        leftAction = {
            rememberPopup()
            leftAction?.invoke()
        },
        rightActions = rememberedRightActions,
        onClick = rememberedOnClick,
        holdDownState = isDropdownExpanded.value,
        enabled = actualEnabled
    )
}
