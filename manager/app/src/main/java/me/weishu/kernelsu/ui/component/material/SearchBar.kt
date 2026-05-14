package me.weishu.kernelsu.ui.component.material

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.text.input.rememberTextFieldState
import androidx.compose.foundation.text.input.setTextAndPlaceCursorAtEnd
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.ExpandedFullScreenContainedSearchBar
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SearchBar
import androidx.compose.material3.SearchBarDefaults
import androidx.compose.material3.SearchBarValue
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.Surface
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberContainedSearchBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import me.weishu.kernelsu.ui.util.LocalSnackbarHost

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun SearchAppBar(
    title: @Composable () -> Unit,
    searchText: String,
    onSearchTextChange: (String) -> Unit,
    onClearClick: () -> Unit,
    navigationIcon: @Composable (() -> Unit)? = null,
    actions: @Composable (() -> Unit)? = null,
    scrollBehavior: TopAppBarScrollBehavior? = null,
    defaultContent: @Composable BoxScope.(bottomPadding: Dp, closeSearch: () -> Unit) -> Unit = { _, _ -> },
    searchContent: @Composable BoxScope.(bottomPadding: Dp, closeSearch: () -> Unit) -> Unit = { _, _ -> }
) {
    val keyboardController = LocalSoftwareKeyboardController.current
    val focusManager = LocalFocusManager.current
    val scaledDensity = LocalDensity.current
    val interactionSource = remember { MutableInteractionSource() }

    val scope = rememberCoroutineScope()
    val searchBarState = rememberContainedSearchBarState()
    val textFieldState = rememberTextFieldState()
    val currentQuery = textFieldState.text.toString()
    val latestSearchText by rememberUpdatedState(searchText)
    val isSearchExpanded = searchBarState.currentValue != SearchBarValue.Collapsed || searchBarState.targetValue != SearchBarValue.Collapsed
    var previousSearchBarValue by remember { mutableStateOf(searchBarState.currentValue) }
    var shouldClearOnCollapse by remember { mutableStateOf(true) }
    val clearSearchText: () -> Unit = {
        textFieldState.setTextAndPlaceCursorAtEnd("")
        onClearClick()
    }
    val collapseAndClear: () -> Unit = {
        shouldClearOnCollapse = false
        clearSearchText()
        scope.launch { searchBarState.animateToCollapsed() }
        focusManager.clearFocus()
        keyboardController?.hide()
    }

    DisposableEffect(Unit) {
        onDispose {
            keyboardController?.hide()
        }
    }

    LaunchedEffect(searchText) {
        val current = textFieldState.text.toString()
        if (current != searchText) {
            textFieldState.setTextAndPlaceCursorAtEnd(searchText)
        }
    }

    LaunchedEffect(textFieldState, latestSearchText) {
        snapshotFlow { textFieldState.text.toString() }
            .distinctUntilChanged()
            .collect { value ->
                if (value != latestSearchText) {
                    onSearchTextChange(value)
                }
            }
    }

    LaunchedEffect(searchBarState) {
        snapshotFlow { searchBarState.currentValue }
            .distinctUntilChanged()
            .collect { value ->
                val collapsedFromExpanded =
                    previousSearchBarValue != SearchBarValue.Collapsed && value == SearchBarValue.Collapsed
                previousSearchBarValue = value
                if (collapsedFromExpanded) {
                    if (shouldClearOnCollapse) {
                        clearSearchText()
                    }
                    shouldClearOnCollapse = true
                    focusManager.clearFocus()
                    keyboardController?.hide()
                }
            }
    }

    BackHandler(isSearchExpanded) {
        if (isSearchExpanded) {
            collapseAndClear()
        }
    }

    val inputField: @Composable () -> Unit = {
        CompositionLocalProvider(LocalDensity provides scaledDensity) {
            SearchBarDefaults.InputField(
                textFieldState = textFieldState,
                searchBarState = searchBarState,
                onSearch = {
                    focusManager.clearFocus()
                    keyboardController?.hide()
                },
                leadingIcon = {
                    if (isSearchExpanded) {
                        IconButton(
                            onClick = { collapseAndClear() },
                            content = { Icon(Icons.AutoMirrored.Filled.ArrowBack, null) }
                        )
                    } else {
                        Icon(Icons.Filled.Search, null)
                    }
                },
                trailingIcon = {
                    if (isSearchExpanded && currentQuery.isNotEmpty()) {
                        IconButton(
                            onClick = { clearSearchText() },
                            content = { Icon(Icons.Filled.Close, null) }
                        )
                    }
                },
                interactionSource = interactionSource
            )
        }
    }

    Surface {
        Column(
            modifier = Modifier.fillMaxWidth()
        ) {
            LargeFlexibleTopAppBar(
                title = title,
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                    scrolledContainerColor = MaterialTheme.colorScheme.surface
                ),
                navigationIcon = { if (navigationIcon != null) navigationIcon() },
                actions = { if (actions != null) actions() },
                windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
                scrollBehavior = scrollBehavior
            )

            SearchBar(
                modifier = Modifier
                    .fillMaxWidth()
                    .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Horizontal))
                    .padding(horizontal = 16.dp)
                    .padding(bottom = 8.dp),
                state = searchBarState,
                inputField = inputField,
            )
        }
    }

    ExpandedFullScreenContainedSearchBar(
        state = searchBarState,
        inputField = inputField,
        windowInsets = { SearchBarDefaults.fullScreenWindowInsets.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal) },
        colors = SearchBarDefaults.colors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
            inputFieldColors = SearchBarDefaults.inputFieldColors(
                focusedContainerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                unfocusedContainerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                disabledContainerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
            )
        ),
        content = {
            val snackBarHostState = LocalSnackbarHost.current
            val bottomPadding = SearchBarDefaults.fullScreenWindowInsets.asPaddingValues().calculateBottomPadding()
            Box(modifier = Modifier.fillMaxSize()) {
                if (currentQuery.isNotEmpty()) {
                    searchContent(bottomPadding, collapseAndClear)
                } else {
                    defaultContent(bottomPadding, collapseAndClear)
                }
                SnackbarHost(
                    hostState = snackBarHostState,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .navigationBarsPadding()
                        .imePadding()
                        .padding(bottom = 16.dp)
                )
            }
        }
    )
}
