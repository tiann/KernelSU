package me.weishu.kernelsu.ui.component

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.core.LinearOutSlowInEasing
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.animation.core.Transition
import androidx.compose.animation.core.animateDp
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandHorizontally
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.shrinkHorizontally
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.max
import androidx.compose.ui.zIndex
import dev.chrisbanes.haze.HazeState
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.hazeEffect
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.InputField
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.basic.Search
import top.yukonga.miuix.kmp.icon.basic.SearchCleanup
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical

// Search Status Class
@Stable
class SearchStatus(val label: String) {
    var searchText by mutableStateOf("")
    val expandState = MutableTransitionState(false)
    var offsetY by mutableStateOf(0.dp)
    var resultStatus by mutableStateOf(ResultStatus.DEFAULT)

    enum class ResultStatus { DEFAULT, EMPTY, LOAD, SHOW }
}

@Composable
fun Transition<Boolean>.TopAppBarAnim(
    modifier: Modifier = Modifier,
    hazeState: HazeState? = null,
    hazeStyle: HazeStyle? = null,
    content: @Composable () -> Unit
) {
    val topAppBarAlpha = animateFloat({ tween(600, easing = LinearOutSlowInEasing) }, "TopAppBarAlphaAnim") {
        if (!it) 1f else 0f
    }
    Box(
        modifier = modifier
    ) {
        Box(
            modifier = Modifier
                .matchParentSize()
                .then(
                    if (hazeState != null && hazeStyle != null) {
                        Modifier
                            .hazeEffect(hazeState) {
                                style = hazeStyle
                                blurRadius = 30.dp
                                noiseFactor = 0f
                            }
                    } else Modifier
                )
        )
        Box(
            modifier = Modifier
                .alpha(topAppBarAlpha.value)
        ) {
            content()
        }
    }
}

// Search Box Composable
@Composable
fun Transition<Boolean>.SearchBox(
    searchStatus: SearchStatus,
    collapseBar: @Composable (SearchStatus) -> Unit = { searchStatus ->
        SearchBarFake(searchStatus.label)
    },
    searchBarTopPadding: Dp = 12.dp,
    contentPadding: PaddingValues = PaddingValues(0.dp),
    hazeState: HazeState,
    hazeStyle: HazeStyle,
    content: @Composable (Dp) -> Unit
) {

    val density = LocalDensity.current
    val layoutDirection = LocalLayoutDirection.current
    val systemBarsPadding = WindowInsets.systemBars.asPaddingValues().calculateTopPadding()
    val searchBarHeight = remember { mutableStateOf(0.dp) }
    val contentTopPadding = contentPadding.calculateTopPadding() + searchBarHeight.value

    val contentOffsetY by animateDp({ tween(300, easing = LinearOutSlowInEasing) }) {
        if (it) systemBarsPadding + 5.dp - contentTopPadding else 0.dp
    }
    val searchBarScaleY by animateFloat({ tween(300, easing = LinearOutSlowInEasing) }) {
        if (it) 0f else 1f
    }
    AnimatedVisibility(
        visible = { !it },
        modifier = Modifier
            .offset(y = contentOffsetY),
        enter = EnterTransition.None,
        exit = ExitTransition.None,
    ) {
        content(contentTopPadding)
    }
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .pointerInput(Unit) {
                detectTapGestures { searchStatus.expandState.targetState = true }
            }
            .padding(
                top = contentPadding.calculateTopPadding()
            )
            .onSizeChanged {
                with(density) {
                    searchBarHeight.value = it.height.toDp()
                }
            }
            .onGloballyPositioned {
                with(density) {
                    it.positionInWindow().y.apply {
                        searchStatus.offsetY = this@apply.toDp()
                    }
                }
            }
            .graphicsLayer {
                scaleY = searchBarScaleY
                transformOrigin = TransformOrigin(
                    pivotFractionX = 0.5f,
                    pivotFractionY = 0.0f
                )
            }
            .hazeEffect(hazeState) {
                style = hazeStyle
                blurRadius = 30.dp
                noiseFactor = 0f
            }
    ) {
        Box(
            modifier = Modifier
                .padding(
                    top = searchBarTopPadding,
                    start = contentPadding.calculateStartPadding(layoutDirection) + 12.dp,
                    end = contentPadding.calculateEndPadding(layoutDirection) + 12.dp,
                    bottom = 6.dp
                )
                .alpha(if (isTransitioning || currentState) 0f else 1f)
        ) {
            collapseBar(searchStatus)
        }
    }
}

// Search Pager Composable
@Composable
fun Transition<Boolean>.SearchPager(
    searchStatus: SearchStatus,
    defaultResult: @Composable () -> Unit,
    expandBar: @Composable (SearchStatus, Dp) -> Unit = { searchStatus, padding ->
        SearchBar(searchStatus, padding)
    },
    searchBarTopPadding: Dp = 12.dp,
    result: LazyListScope.() -> Unit
) {
    val systemBarsPadding = WindowInsets.systemBars.asPaddingValues().calculateTopPadding()
    val topPadding by animateDp({ tween(300, easing = LinearOutSlowInEasing) }) {
        if (it) {
            systemBarsPadding + 5.dp
        } else {
            max(searchStatus.offsetY, 0.dp)
        }
    }
    val surfaceAlpha by animateFloat({ tween(150, easing = LinearOutSlowInEasing) }) {
        if (it) 1f else 0f
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .zIndex(1f)
            .background(colorScheme.surface.copy(alpha = surfaceAlpha))
            .semantics { onClick { false } }
            .then(
                if (this@SearchPager.targetState) Modifier.pointerInput(Unit) { } else Modifier
            ),
    ) {
        Row(
            Modifier
                .fillMaxWidth()
                .padding(top = topPadding, bottom = 6.dp),
            horizontalArrangement = Arrangement.Start,
            verticalAlignment = Alignment.CenterVertically
        ) {

            if (this@SearchPager.isTransitioning || this@SearchPager.currentState) {
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .padding(horizontal = 12.dp)
                ) {
                    expandBar(searchStatus, searchBarTopPadding)
                }
            }

            this@SearchPager.AnimatedVisibility(
                visible = { it },
                modifier = Modifier.wrapContentHeight(),
                enter = expandHorizontally() + slideInHorizontally(initialOffsetX = { it }),
                exit = shrinkHorizontally() + slideOutHorizontally(targetOffsetX = { it })
            ) {
                Text(
                    text = stringResource(android.R.string.cancel),
                    fontWeight = FontWeight.Bold,
                    color = colorScheme.primary,
                    modifier = Modifier
                        .padding(start = 4.dp, end = 16.dp, top = searchBarTopPadding)
                        .clickable(
                            interactionSource = null,
                            indication = null
                        ) {
                            searchStatus.apply {
                                searchText = ""
                                expandState.targetState = false
                            }
                        }
                )
                BackHandler(enabled = true) {
                    searchStatus.apply {
                        searchText = ""
                        expandState.targetState = false
                    }
                }
            }
        }
        this@SearchPager.AnimatedVisibility(
            visible = { it },
            modifier = Modifier.fillMaxSize(),
            enter = fadeIn(),
            exit = fadeOut()
        ) {
            when (searchStatus.resultStatus) {
                SearchStatus.ResultStatus.DEFAULT -> defaultResult()
                SearchStatus.ResultStatus.EMPTY -> {}
                SearchStatus.ResultStatus.LOAD -> {}
                SearchStatus.ResultStatus.SHOW -> LazyColumn(
                    Modifier
                        .fillMaxSize()
                        .overScrollVertical(),
                ) {
                    result()
                }
            }
        }
    }
}

@Composable
fun SearchBar(
    searchStatus: SearchStatus,
    searchBarTopPadding: Dp = 12.dp,
) {
    val focusRequester = remember { FocusRequester() }

    InputField(
        query = searchStatus.searchText,
        onQueryChange = { searchStatus.searchText = it },
        label = "",
        leadingIcon = {
            Icon(
                imageVector = MiuixIcons.Basic.Search,
                contentDescription = "back",
                modifier = Modifier
                    .size(44.dp)
                    .padding(start = 16.dp, end = 8.dp),
                tint = colorScheme.onSurfaceContainerHigh,
            )
        },
        trailingIcon = {
            AnimatedVisibility(
                searchStatus.searchText.isNotEmpty(),
                enter = fadeIn() + scaleIn(),
                exit = fadeOut() + scaleOut(),
            ) {
                Icon(
                    imageVector = MiuixIcons.Basic.SearchCleanup,
                    tint = colorScheme.onSurface,
                    contentDescription = "Clean",
                    modifier = Modifier
                        .size(44.dp)
                        .padding(start = 8.dp, end = 16.dp)
                        .clickable(
                            interactionSource = null,
                            indication = null
                        ) {
                            searchStatus.searchText = ""
                        },
                )
            }
        },
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = searchBarTopPadding)
            .focusRequester(focusRequester),
        onSearch = { it },
        expanded = searchStatus.expandState.currentState,
        onExpandedChange = {
            searchStatus.expandState.targetState = it
        }
    )
    LaunchedEffect(Unit) {
        if (searchStatus.expandState.targetState) {
            focusRequester.requestFocus()
        }
    }
}

@Composable
fun SearchBarFake(
    label: String,
) {
    InputField(
        query = "",
        onQueryChange = { },
        label = label,
        leadingIcon = {
            Icon(
                imageVector = MiuixIcons.Basic.Search,
                contentDescription = "Clean",
                modifier = Modifier
                    .size(44.dp)
                    .padding(start = 16.dp, end = 8.dp),
                tint = colorScheme.onSurfaceContainerHigh,
            )
        },
        modifier = Modifier,
        onSearch = { },
        enabled = false,
        expanded = false,
        onExpandedChange = { }
    )
}
