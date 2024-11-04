package me.weishu.kernelsu.ui.component

import android.content.res.Configuration
import androidx.activity.compose.BackHandler
import androidx.activity.compose.LocalOnBackPressedDispatcherOwner
import androidx.compose.animation.Animatable
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.constraintlayout.compose.ConstraintLayout

private const val TAG = "SearchBar"

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SearchAppBar(
    title: @Composable () -> Unit,
    searchText: String,
    onSearchTextChange: (String) -> Unit,
    onClearClick: () -> Unit,
    onBackClick: (() -> Unit)? = null,
    onConfirm: (() -> Unit)? = null,
    dropdownContent: @Composable (() -> Unit)? = null,
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    val keyboardController = LocalSoftwareKeyboardController.current
    var onSearch by rememberSaveable { mutableStateOf(false) }
    val focusRequester = remember { FocusRequester() }
    val backDispatcher = LocalOnBackPressedDispatcherOwner.current?.onBackPressedDispatcher

    AnimatedContent(targetState = onSearch, label = "SearchAppBarToggle") { state ->
        if (!state) return@AnimatedContent TopAppBar(
            title = title,
            navigationIcon = {
                onBackClick?.let {
                    IconButton(
                        onClick = it,
                        content = { Icon(Icons.AutoMirrored.Outlined.ArrowBack, null) }
                    )
                }
            },
            actions = {
                IconButton(
                    onClick = { onSearch = true },
                    content = { Icon(Icons.Filled.Search, null) }
                )
                dropdownContent?.invoke()
            },
            scrollBehavior = scrollBehavior
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .statusBarsPadding(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            SearchBox(
                searchText = searchText,
                onSearchTextChange = onSearchTextChange,
                onClearClick = {
                    backDispatcher?.onBackPressed()
                    onClearClick.invoke()
                },
                modifier = Modifier
                    .weight(1f)
                    .focusRequester(focusRequester),
                keyboardActions = KeyboardActions(onDone = {
                    keyboardController?.hide()
                    onConfirm?.invoke()
                })
            )
            dropdownContent?.invoke()
            DisposableEffect(Unit) {
                onDispose {
                    onSearchTextChange.invoke("")
                }
            }
        }
    }
    BackHandler(onSearch) { onSearch = false }

    LaunchedEffect(onSearch) {
        if (onSearch) {
            focusRequester.requestFocus()
        }
    }
}


@Composable
private fun SearchBox(
    searchText: String,
    onSearchTextChange: (String) -> Unit,
    onClearClick: () -> Unit,
    modifier: Modifier = Modifier,
    keyboardActions: KeyboardActions = KeyboardActions()
) {
    BasicTextField(
        value = searchText,
        onValueChange = onSearchTextChange,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp)
            .then(modifier),
        textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
        singleLine = true,
        keyboardOptions = KeyboardOptions.Default.copy(imeAction = ImeAction.Done),
        keyboardActions = keyboardActions,
        cursorBrush = SolidColor(MaterialTheme.colorScheme.onSurface)
    ) { innerTF ->
        ConstraintLayout(
            modifier = Modifier
                .padding(8.dp)
                .border(1.5f.dp, MaterialTheme.colorScheme.outline, MaterialTheme.shapes.extraLarge)
        ) {
            val (leadingIconRef, textFieldRef, trailingIconRef) = createRefs()

            IconButton(
                onClick = {},
                modifier = Modifier.constrainAs(leadingIconRef) {
                    start.linkTo(parent.start)
                    centerVerticallyTo(parent)
                },
                enabled = false
            ) {
                Icon(
                    imageVector = Icons.Filled.Search,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurface
                )
            }
            Box(
                modifier = Modifier.constrainAs(textFieldRef) {
                    start.linkTo(leadingIconRef.end)
                    centerVerticallyTo(parent)
                },
                content = { innerTF.invoke() }
            )
            IconButton(
                onClick = onClearClick,
                modifier = Modifier.constrainAs(trailingIconRef) {
                    end.linkTo(parent.end)
                    centerVerticallyTo(parent)
                }
            ) {
                Icon(
                    imageVector = Icons.Filled.Close,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurface
                )
            }
        }
    }
}

@Preview(
    showBackground = true,
    uiMode = Configuration.UI_MODE_NIGHT_YES or Configuration.UI_MODE_TYPE_NORMAL
)
@Composable
private fun SearchBoxPreview() {
    var searchText by rememberSaveable { mutableStateOf("") }
    SearchBox(
        searchText = searchText,
        onSearchTextChange = { searchText = it },
        onClearClick = { searchText = "" }
    )
}


@OptIn(ExperimentalMaterial3Api::class)
@Preview
@Composable
private fun SearchAppBarPreview() {
    var searchText by remember { mutableStateOf("") }
    SearchAppBar(
        title = { Text("Search text") },
        searchText = searchText,
        onSearchTextChange = { searchText = it },
        onClearClick = { searchText = "" }
    )
}
