package me.weishu.kernelsu.ui.screen

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.pullrefresh.PullRefreshIndicator
import androidx.compose.material.pullrefresh.pullRefresh
import androidx.compose.material.pullrefresh.rememberPullRefreshState
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.compose.AsyncImage
import coil.request.ImageRequest
import com.ramcosta.composedestinations.annotation.Destination
import kotlinx.coroutines.launch
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.ConfirmDialog
import me.weishu.kernelsu.ui.component.ConfirmResult
import me.weishu.kernelsu.ui.component.SearchAppBar
import me.weishu.kernelsu.ui.util.LocalDialogHost
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import java.util.*

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterialApi::class)
@Destination
@Composable
fun SuperUserScreen() {
    val viewModel = viewModel<SuperUserViewModel>()
    val snackbarHost = LocalSnackbarHost.current
    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        if (viewModel.appList.isEmpty()) {
            viewModel.fetchAppList()
        }
    }

    Scaffold(
        topBar = {
            SearchAppBar(
                title = { Text(stringResource(R.string.superuser)) },
                searchText = viewModel.search,
                onSearchTextChange = { viewModel.search = it },
                onClearClick = { viewModel.search = "" },
                dropdownContent = {
                    var showDropdown by remember { mutableStateOf(false) }

                    IconButton(
                        onClick = { showDropdown = true },
                    ) {
                        Icon(
                            imageVector = Icons.Filled.MoreVert,
                            contentDescription = stringResource(id = R.string.settings)
                        )

                        DropdownMenu(expanded = showDropdown, onDismissRequest = {
                            showDropdown = false
                        }) {
                            DropdownMenuItem(text = {
                                Text(stringResource(R.string.refresh))
                            }, onClick = {
                                scope.launch {
                                    viewModel.fetchAppList()
                                }
                                showDropdown = false
                            })
                            DropdownMenuItem(text = {
                                Text(
                                    if (viewModel.showSystemApps) {
                                        stringResource(R.string.hide_system_apps)
                                    } else {
                                        stringResource(R.string.show_system_apps)
                                    }
                                )
                            }, onClick = {
                                viewModel.showSystemApps = !viewModel.showSystemApps
                                showDropdown = false
                            })
                        }
                    }
                },
            )
        }
    ) { innerPadding ->

        ConfirmDialog()

        val refreshState = rememberPullRefreshState(
            refreshing = viewModel.isRefreshing,
            onRefresh = { scope.launch { viewModel.fetchAppList() } },
        )
        Box(
            modifier = Modifier
                .padding(innerPadding)
                .pullRefresh(refreshState)
        ) {
            val failMessage = stringResource(R.string.superuser_failed_to_grant_root)

            LazyColumn(Modifier.fillMaxSize()) {
                items(viewModel.appList, key = { it.packageName + it.uid }) { app ->
                    var isChecked by rememberSaveable(app) { mutableStateOf(app.onAllowList) }
                    val dialogHost = LocalDialogHost.current
                    val content =
                        stringResource(id = R.string.superuser_allow_root_confirm, app.label)
                    val confirm = stringResource(id = android.R.string.ok)
                    val cancel = stringResource(id = android.R.string.cancel)

                    AppItem(app, isChecked) { checked ->
                        scope.launch {
                            if (checked) {
                                val confirmResult = dialogHost.showConfirm(
                                    app.label,
                                    content = content,
                                    confirm = confirm,
                                    dismiss = cancel
                                )
                                if (confirmResult != ConfirmResult.Confirmed) {
                                    return@launch
                                }
                            }

                            val success = Natives.allowRoot(app.uid, checked)
                            if (success) {
                                isChecked = checked
                            } else {
                                snackbarHost.showSnackbar(failMessage.format(app.uid))
                            }
                        }
                    }
                }
            }

            PullRefreshIndicator(
                refreshing = viewModel.isRefreshing,
                state = refreshState,
                modifier = Modifier.align(Alignment.TopCenter)
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AppItem(
    app: SuperUserViewModel.AppInfo,
    isChecked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    ListItem(
        headlineText = { Text(app.label) },
        supportingText = { Text(app.packageName) },
        leadingContent = {
            AsyncImage(
                model = ImageRequest.Builder(LocalContext.current)
                    .data(app.icon)
                    .crossfade(true)
                    .build(),
                contentDescription = app.label,
                modifier = Modifier
                    .padding(4.dp)
                    .width(48.dp)
                    .height(48.dp)
            )
        },
        trailingContent = {
            Switch(
                checked = isChecked,
                onCheckedChange = onCheckedChange,
                modifier = Modifier.padding(4.dp)
            )
        }
    )
}
