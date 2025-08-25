package me.weishu.kernelsu.ui.screen

import android.widget.Toast
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.captionBar
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Fingerprint
import androidx.compose.material.icons.outlined.Group
import androidx.compose.material.icons.outlined.Shield
import androidx.compose.material.icons.rounded.Add
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.generated.destinations.TemplateEditorScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.ramcosta.composedestinations.result.ResultRecipient
import com.ramcosta.composedestinations.result.getOr
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.DropdownItem
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.FloatingActionButton
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopup
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.ScrollBehavior
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.icon.icons.useful.Copy
import top.yukonga.miuix.kmp.icon.icons.useful.Refresh
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.PressFeedbackType
import top.yukonga.miuix.kmp.utils.getWindowSize
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

/**
 * @author weishu
 * @date 2023/10/20.
 */
@Composable
@Destination<RootGraph>
fun AppProfileTemplateScreen(
    navigator: DestinationsNavigator,
    resultRecipient: ResultRecipient<TemplateEditorScreenDestination, Boolean>
) {
    val viewModel = viewModel<TemplateViewModel>()
    val scope = rememberCoroutineScope()
    val scrollBehavior = MiuixScrollBehavior()

    LaunchedEffect(Unit) {
        if (viewModel.templateList.isEmpty()) {
            viewModel.fetchTemplates()
        }
    }

    // handle result from TemplateEditorScreen, refresh if needed
    resultRecipient.onNavResult { result ->
        if (result.getOr { false }) {
            scope.launch { viewModel.fetchTemplates() }
        }
    }

    val listState = rememberLazyListState()
    var fabVisible by remember { mutableStateOf(true) }
    var scrollDistance by remember { mutableFloatStateOf(0f) }
    val nestedScrollConnection = remember {
        object : NestedScrollConnection {
            override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
                val isScrolledToEnd =
                    (listState.layoutInfo.visibleItemsInfo.lastOrNull()?.index == listState.layoutInfo.totalItemsCount - 1
                            && (listState.layoutInfo.visibleItemsInfo.lastOrNull()?.size
                        ?: 0) < listState.layoutInfo.viewportEndOffset)
                val delta = available.y
                if (!isScrolledToEnd) {
                    scrollDistance += delta
                    if (scrollDistance < -50f) {
                        if (fabVisible) fabVisible = false
                        scrollDistance = 0f
                    } else if (scrollDistance > 50f) {
                        if (!fabVisible) fabVisible = true
                        scrollDistance = 0f
                    }
                }
                return Offset.Zero
            }
        }
    }
    val offsetHeight by animateDpAsState(
        targetValue = if (fabVisible) 0.dp else 100.dp + WindowInsets.systemBars.asPaddingValues().calculateBottomPadding(),
        animationSpec = tween(durationMillis = 350)
    )

    Scaffold(
        topBar = {
            val clipboardManager = LocalClipboardManager.current
            val context = LocalContext.current
            val showToast = fun(msg: String) {
                scope.launch(Dispatchers.Main) {
                    Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
                }
            }
            TopBar(
                onBack = dropUnlessResumed { navigator.popBackStack() },
                onSync = {
                    scope.launch { viewModel.fetchTemplates(true) }
                },
                onImport = {
                    clipboardManager.getText()?.text?.let {
                        if (it.isEmpty()) {
                            showToast(context.getString(R.string.app_profile_template_import_empty))
                            return@let
                        }
                        scope.launch {
                            viewModel.importTemplates(
                                it, {
                                    showToast(context.getString(R.string.app_profile_template_import_success))
                                    viewModel.fetchTemplates(false)
                                },
                                showToast
                            )
                        }
                    }
                },
                onExport = {
                    scope.launch {
                        viewModel.exportTemplates(
                            {
                                showToast(context.getString(R.string.app_profile_template_export_empty))
                            }
                        ) {
                            clipboardManager.setText(AnnotatedString(it))
                        }
                    }
                },
                scrollBehavior = scrollBehavior,
            )
        },
        floatingActionButton = {
            FloatingActionButton(
                containerColor = colorScheme.primary,
                shadowElevation = 0.dp,
                onClick = {
                    navigator.navigate(TemplateEditorScreenDestination(TemplateViewModel.TemplateInfo(), false)) {
                        launchSingleTop = true
                    }
                },
                modifier = Modifier
                    .offset(y = offsetHeight)
                    .padding(
                        bottom = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                WindowInsets.captionBar.asPaddingValues().calculateBottomPadding() + 20.dp,
                        end = 20.dp
                    )
                    .border(0.05.dp, colorScheme.outline.copy(alpha = 0.5f), CircleShape),
                content = {
                    Icon(
                        Icons.Rounded.Add,
                        null,
                        Modifier.size(40.dp),
                        tint = Color.White
                    )
                },
            )
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        var isRefreshing by rememberSaveable { mutableStateOf(false) }
        val pullToRefreshState = rememberPullToRefreshState()
        LaunchedEffect(isRefreshing) {
            if (isRefreshing) {
                delay(350)
                viewModel.fetchTemplates()
                isRefreshing = false
            }
        }
        val refreshTexts = listOf(
            stringResource(R.string.refresh_pulling),
            stringResource(R.string.refresh_release),
            stringResource(R.string.refresh_refresh),
            stringResource(R.string.refresh_complete),
        )
        val layoutDirection = LocalLayoutDirection.current
        PullToRefresh(
            isRefreshing = isRefreshing,
            pullToRefreshState = pullToRefreshState,
            onRefresh = { isRefreshing = true },
            refreshTexts = refreshTexts,
            contentPadding = PaddingValues(
                top = innerPadding.calculateTopPadding() + 12.dp,
                start = innerPadding.calculateStartPadding(layoutDirection),
                end = innerPadding.calculateEndPadding(layoutDirection)
            ),
        ) {
            LazyColumn(
                modifier = Modifier
                    .height(getWindowSize().height.dp)
                    .scrollEndHaptic()
                    .overScrollVertical()
                    .nestedScroll(nestedScrollConnection)
                    .nestedScroll(scrollBehavior.nestedScrollConnection)
                    .padding(horizontal = 12.dp),
                contentPadding = innerPadding,
                overscrollEffect = null
            ) {
                item {
                    Spacer(Modifier.height(12.dp))
                }
                items(viewModel.templateList, key = { it.id }) { app ->
                    TemplateItem(navigator, app)
                }
                item {
                    Spacer(
                        Modifier.height(
                            WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() +
                                    WindowInsets.captionBar.asPaddingValues().calculateBottomPadding()
                        )
                    )
                }
            }
        }
    }
}

@Composable
private fun TemplateItem(
    navigator: DestinationsNavigator,
    template: TemplateViewModel.TemplateInfo
) {
    Card(
        modifier = Modifier.padding(bottom = 12.dp),
        onClick = {
            navigator.navigate(TemplateEditorScreenDestination(template, !template.local)) {
                popUpTo(TemplateEditorScreenDestination) {
                    inclusive = true
                }
                launchSingleTop = true
            }
        },
        showIndication = true,
        pressFeedbackType = PressFeedbackType.Sink
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = template.name,
                    fontSize = 17.sp,
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurface,
                )
                Spacer(modifier = Modifier.weight(1f))
                if (template.local) {
                    Text(
                        text = "LOCAL",
                        color = colorScheme.onTertiaryContainer,
                        fontWeight = FontWeight.Bold,
                        style = MiuixTheme.textStyles.footnote1
                    )
                } else {
                    Text(
                        text = "REMOTE",
                        color = colorScheme.onSurfaceSecondary,
                        fontWeight = FontWeight.Bold,
                        style = MiuixTheme.textStyles.footnote1
                    )
                }
            }

            Text(
                text = "${template.id}${if (template.author.isEmpty()) "" else " by @${template.author}"}",
                modifier = Modifier.padding(top = 1.dp),
                fontSize = 14.sp,
                color = colorScheme.onSurfaceVariantSummary,
                fontWeight = FontWeight.Medium,
            )

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = template.description,
                fontSize = 14.5.sp,
                color = colorScheme.onSurfaceVariantSummary,
            )

            HorizontalDivider(
                modifier = Modifier.padding(vertical = 8.dp),
                thickness = 0.5.dp,
                color = colorScheme.outline.copy(alpha = 0.5f)
            )

            FlowRow(
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                InfoChip(
                    icon = Icons.Outlined.Fingerprint,
                    text = "UID: ${template.uid}"
                )
                InfoChip(
                    icon = Icons.Outlined.Group,
                    text = "GID: ${template.gid}"
                )
                InfoChip(
                    icon = Icons.Outlined.Shield,
                    text = template.context
                )
            }
        }
    }
}

@Composable
private fun InfoChip(icon: ImageVector, text: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            modifier = Modifier.size(14.dp),
            tint = colorScheme.onSurfaceSecondary.copy(alpha = 0.8f)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = text,
            style = MiuixTheme.textStyles.body2,
            color = colorScheme.onSurfaceSecondary
        )
    }
}

@Composable
private fun TopBar(
    onBack: () -> Unit,
    onSync: () -> Unit = {},
    onImport: () -> Unit = {},
    onExport: () -> Unit = {},
    scrollBehavior: ScrollBehavior,
) {
    TopAppBar(
        title = stringResource(R.string.settings_profile_template),
        navigationIcon = {
            IconButton(
                modifier = Modifier.padding(start = 16.dp),
                onClick = onBack
            ) {
                Icon(
                    imageVector = MiuixIcons.Useful.Back,
                    contentDescription = null,
                    tint = colorScheme.onBackground
                )
            }
        },
        actions = {
            IconButton(
                modifier = Modifier.padding(end = 16.dp),
                onClick = onSync
            ) {
                Icon(
                    imageVector = MiuixIcons.Useful.Refresh,
                    contentDescription = stringResource(id = R.string.app_profile_template_sync),
                    tint = colorScheme.onBackground
                )
            }

            val showTopPopup = remember { mutableStateOf(false) }
            ListPopup(
                show = showTopPopup,
                popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
                alignment = PopupPositionProvider.Align.TopRight,
                onDismissRequest = {
                    showTopPopup.value = false
                }
            ) {
                ListPopupColumn {
                    val items = listOf(
                        stringResource(id = R.string.app_profile_import_from_clipboard),
                        stringResource(id = R.string.app_profile_export_to_clipboard)
                    )
                    items.forEachIndexed { index, text ->
                        DropdownItem(
                            text = text,
                            optionSize = items.size,
                            index = index,
                            onSelectedIndexChange = { selectedIndex ->
                                if (selectedIndex == 0) {
                                    onImport()
                                } else {
                                    onExport()
                                }
                                showTopPopup.value = false
                            }
                        )
                    }
                }
            }
            IconButton(
                modifier = Modifier.padding(end = 16.dp),
                onClick = { showTopPopup.value = true },
                holdDownState = showTopPopup.value
            ) {
                Icon(
                    imageVector = MiuixIcons.Useful.Copy,
                    contentDescription = stringResource(id = R.string.app_profile_import_export),
                    tint = colorScheme.onBackground
                )
            }
        },
        scrollBehavior = scrollBehavior
    )
}
