package me.weishu.kernelsu.ui.component.material

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.relocation.BringIntoViewRequester
import androidx.compose.foundation.relocation.bringIntoViewRequester
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ProvideTextStyle
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.surfaceColorAtElevation
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch

private val largeCorner = 20.dp
private val smallCorner = 4.dp

@Composable
private fun ExpressiveItemWrapper(
    isSelected: Boolean,
    isFirst: Boolean,
    isLast: Boolean,
    content: @Composable () -> Unit
) {
    val targetTop = if (isSelected || isFirst) largeCorner else smallCorner
    val targetBottom = if (isSelected || isLast) largeCorner else smallCorner

    val topStart by animateDpAsState(targetTop, label = "topStart")
    val topEnd by animateDpAsState(targetTop, label = "topEnd")
    val bottomStart by animateDpAsState(targetBottom, label = "bottomStart")
    val bottomEnd by animateDpAsState(targetBottom, label = "bottomEnd")

    val shape = RoundedCornerShape(
        topStart = topStart,
        topEnd = topEnd,
        bottomEnd = bottomEnd,
        bottomStart = bottomStart
    )

    val backgroundColor by animateColorAsState(
        if (isSelected) MaterialTheme.colorScheme.secondaryContainer
        else MaterialTheme.colorScheme.surfaceColorAtElevation(1.dp),
        label = "backgroundColor"
    )

    val contentColor by animateColorAsState(
        if (isSelected) MaterialTheme.colorScheme.onSecondaryContainer
        else MaterialTheme.colorScheme.onSurface,
        label = "contentColor"
    )

    Column(
        modifier = Modifier
            .clip(shape)
            .background(backgroundColor)
    ) {
        CompositionLocalProvider(LocalContentColor provides contentColor) {
            content()
        }
    }
}

@Composable
fun ExpressiveColumn(
    modifier: Modifier = Modifier,
    title: String = "",
    selectedIndices: Set<Int> = emptySet(),
    content: List<@Composable () -> Unit>,
) {
    if (content.isEmpty()) return

    Column(modifier = modifier) {
        if (title.isNotEmpty()) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(start = 16.dp, bottom = 8.dp)
            )
        }
        Column(
            modifier = Modifier.clip(RoundedCornerShape(largeCorner)),
            verticalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            content.forEachIndexed { index, itemContent ->
                ExpressiveItemWrapper(
                    isSelected = selectedIndices.contains(index),
                    isFirst = index == 0,
                    isLast = index == content.size - 1,
                    content = itemContent
                )
            }
        }
    }
}

@Composable
fun <T> ExpressiveLazyColumn(
    modifier: Modifier = Modifier,
    state: LazyListState = rememberLazyListState(),
    contentPadding: PaddingValues = PaddingValues(all = 16.dp),
    title: String = "",
    key: ((T) -> Any)? = null,
    selected: (T) -> Boolean = { false },
    items: List<T>,
    itemContent: @Composable (T) -> Unit
) {
    Column(modifier = modifier) {
        if (title.isNotEmpty()) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(start = 16.dp, bottom = 8.dp)
            )
        }
        LazyColumn(
            state = state,
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(2.dp),
            contentPadding = contentPadding
        ) {
            itemsIndexed(
                items = items,
                key = if (key != null) { _, item -> key(item) } else null
            ) { index, item ->
                ExpressiveItemWrapper(
                    isSelected = selected(item),
                    isFirst = index == 0,
                    isLast = index == items.size - 1,
                    content = { itemContent(item) }
                )
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun ExpressiveListItem(
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
    onLongClick: (() -> Unit)? = null,
    headlineContent: @Composable () -> Unit,
    supportingContent: @Composable (() -> Unit)? = null,
    leadingContent: @Composable (() -> Unit)? = null,
    trailingContent: @Composable (() -> Unit)? = null,
    bottomContent: @Composable (() -> Unit)? = null,
) {
    Column(modifier = Modifier
        .fillMaxWidth()
        .let {
            if (onClick != null || onLongClick != null) {
                it.combinedClickable(onClick = onClick ?: {}, onLongClick = onLongClick)
            } else {
                it
            }
        }
        .then(modifier)
        .padding(horizontal = 16.dp, vertical = 8.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically
        ) {
            if (leadingContent != null) {
                Box(
                    modifier = Modifier.padding(end = 16.dp),
                    contentAlignment = Alignment.Center
                ) {
                    leadingContent()
                }
            }
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(vertical = 8.dp)
            ) {
                headlineContent()
                if (supportingContent != null) {
                    CompositionLocalProvider(
                        LocalContentColor provides MaterialTheme.colorScheme.outline
                    ) {
                        ProvideTextStyle(value = MaterialTheme.typography.bodyMedium) {
                            supportingContent()
                        }
                    }
                }
            }
            if (trailingContent != null) {
                Box(
                    modifier = Modifier.padding(start = 16.dp),
                    contentAlignment = Alignment.Center
                ) {
                    ProvideTextStyle(value = MaterialTheme.typography.bodyMedium) {
                        trailingContent()
                    }
                }
            }
        }
        if (bottomContent != null) {
            bottomContent()
        }
    }
}

@Composable
fun ExpressiveSwitchItem(
    icon: ImageVector? = null,
    title: String,
    summary: String? = null,
    checked: Boolean,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
) {
    val interactionSource = remember { MutableInteractionSource() }

    ExpressiveListItem(
        onClick = { onCheckedChange(!checked) },
        modifier = Modifier.toggleable(
            value = checked,
            interactionSource = interactionSource,
            role = Role.Switch,
            enabled = enabled,
            indication = LocalIndication.current,
            onValueChange = onCheckedChange
        ),
        headlineContent = { Text(title) },
        leadingContent = icon?.let { { Icon(it, title) } },
        trailingContent = {
            ExpressiveSwitch(
                checked = checked,
                enabled = enabled,
                onCheckedChange = onCheckedChange,
                interactionSource = interactionSource,
            )
        },
        supportingContent = summary?.let { { Text(it) } }
    )
}

@Composable
fun ExpressiveDropdownItem(
    icon: ImageVector? = null,
    title: String,
    summary: String? = null,
    items: List<String>,
    enabled: Boolean = true,
    selectedIndex: Int,
    onItemSelected: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    val hasItems = items.isNotEmpty()
    val safeIndex = if (hasItems) {
        selectedIndex.coerceIn(0, items.lastIndex)
    } else {
        -1
    }

    ExpressiveListItem(
        modifier = if (enabled) {
            Modifier.clickable { expanded = true }
        } else {
            Modifier
        },
        leadingContent = icon?.let { { Icon(it, title) } },
        headlineContent = { Text(text = title) },
        supportingContent = summary?.let { { Text(it) } },
        trailingContent = {
            Box(modifier = Modifier.wrapContentSize(Alignment.TopStart)) {
                Text(
                    text = if (hasItems && safeIndex >= 0) items[safeIndex] else "",
                    textAlign = TextAlign.End,
                    modifier = Modifier.fillMaxWidth(0.3f),
                    color = if (enabled) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant
                )
                DropdownMenu(
                    expanded = expanded,
                    onDismissRequest = { expanded = false }
                ) {
                    items.forEachIndexed { index, text ->
                        DropdownMenuItem(
                            text = { Text(text) },
                            onClick = {
                                if (index in items.indices) {
                                    onItemSelected(index)
                                }
                                expanded = false
                            }
                        )
                    }
                }
            }
        }
    )
}

@Composable
fun ExpressiveRadioItem(
    title: String,
    summary: String? = null,
    selected: Boolean,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    ExpressiveListItem(
        onClick = onClick,
        modifier = Modifier.toggleable(
            value = selected,
            onValueChange = { onClick() },
            enabled = enabled,
            role = Role.RadioButton
        ),
        headlineContent = { Text(title) },
        leadingContent = {
            RadioButton(
                selected = selected,
                onClick = null,
                enabled = enabled
            )
        },
        supportingContent = summary?.let { { Text(it) } }
    )
}

@Composable
fun ExpressiveCheckboxItem(
    title: String,
    summary: String? = null,
    checked: Boolean,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
) {
    val interactionSource = remember { MutableInteractionSource() }

    ExpressiveListItem(
        onClick = { onCheckedChange(!checked) },
        modifier = Modifier.toggleable(
            value = checked,
            interactionSource = interactionSource,
            role = Role.Checkbox,
            enabled = enabled,
            indication = LocalIndication.current,
            onValueChange = onCheckedChange
        ),
        headlineContent = { Text(title) },
        leadingContent = {
            Checkbox(
                checked = checked,
                enabled = enabled,
                onCheckedChange = onCheckedChange,
                interactionSource = interactionSource,
                modifier = Modifier.size(24.dp)
            )
        },
        supportingContent = summary?.let { { Text(it) } }
    )
}

@Composable
fun ExpressiveTextField(
    modifier: Modifier = Modifier,
    label: String = "",
    value: String,
    onValueChange: (String) -> Unit,
    enabled: Boolean = true,
    readOnly: Boolean = false,
    textStyle: TextStyle = MaterialTheme.typography.bodyLarge,
    keyboardOptions: KeyboardOptions = KeyboardOptions.Default,
    keyboardActions: KeyboardActions = KeyboardActions.Default,
    singleLine: Boolean = false,
    maxLines: Int = if (singleLine) 1 else Int.MAX_VALUE,
    minLines: Int = 1,
    visualTransformation: VisualTransformation = VisualTransformation.None,
    onTextLayout: (TextLayoutResult) -> Unit = {},
    interactionSource: MutableInteractionSource = remember { MutableInteractionSource() },
    cursorBrush: Brush = SolidColor(MaterialTheme.colorScheme.primary),
    placeholder: @Composable (() -> Unit)? = { Text("-") },
    leadingContent: @Composable (() -> Unit)? = null,
    trailingContent: @Composable (() -> Unit)? = null,
    supportingContent: @Composable (() -> Unit)? = null,
    isError: Boolean = false
) {
    val bringIntoViewRequester = remember { BringIntoViewRequester() }
    val coroutineScope = rememberCoroutineScope()

    ExpressiveListItem(
        modifier = modifier.bringIntoViewRequester(bringIntoViewRequester),
        leadingContent = leadingContent,
        supportingContent = supportingContent,
        trailingContent = trailingContent,
        headlineContent = {
            Column {
                if (label.isNotEmpty()) {
                    Text(text = label, color = if (isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface)
                }
                BasicTextField(
                    value = value,
                    onValueChange = onValueChange,
                    modifier = Modifier
                        .fillMaxWidth()
                        .onFocusChanged {
                            if (it.isFocused) {
                                coroutineScope.launch {
                                    bringIntoViewRequester.bringIntoView()
                                }
                            }
                        },
                    enabled = enabled,
                    readOnly = readOnly,
                    textStyle = textStyle.copy(MaterialTheme.colorScheme.outline, fontSize = MaterialTheme.typography.bodyMedium.fontSize),
                    keyboardOptions = keyboardOptions,
                    keyboardActions = keyboardActions,
                    singleLine = singleLine,
                    maxLines = maxLines,
                    minLines = minLines,
                    visualTransformation = visualTransformation,
                    onTextLayout = onTextLayout,
                    interactionSource = interactionSource,
                    cursorBrush = cursorBrush,
                    decorationBox = { innerTextField ->
                        if (value.isEmpty() && placeholder != null) {
                            Box(contentAlignment = Alignment.CenterStart) {
                                CompositionLocalProvider(
                                    LocalContentColor provides MaterialTheme.colorScheme.outline
                                ) {
                                    ProvideTextStyle(value = MaterialTheme.typography.bodyMedium) {
                                        placeholder()
                                    }
                                }
                            }
                        }
                        innerTextField()
                    }
                )
            }
        }
    )
}
