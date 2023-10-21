package me.weishu.kernelsu.ui.screen

import android.util.Log
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Create
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material.pullrefresh.PullRefreshIndicator
import androidx.compose.material.pullrefresh.pullRefresh
import androidx.compose.material.pullrefresh.rememberPullRefreshState
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.viewmodel.compose.viewModel
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.screen.destinations.TemplateEditorScreenDestination
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel
import java.util.Locale

/**
 * @author weishu
 * @date 2023/10/20.
 */

@OptIn(ExperimentalMaterialApi::class)
@Destination
@Composable
fun AppProfileTemplateScreen(navigator: DestinationsNavigator) {
    val viewModel = viewModel<TemplateViewModel>()
    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        if (viewModel.templateList.isEmpty()) {
            viewModel.fetchTemplates()
        }
    }

    Scaffold(
        topBar = {
            TopBar(onBack = { navigator.popBackStack() },
                onSync = {
                    scope.launch { viewModel.fetchTemplates(true) }
                },
                onClickCreate = {
                    navigator.navigate(
                        TemplateEditorScreenDestination(
                            TemplateViewModel.TemplateInfo(),
                            false
                        )
                    )
                })
        },
    ) { innerPadding ->
        val refreshState = rememberPullRefreshState(
            refreshing = viewModel.isRefreshing,
            onRefresh = { scope.launch { viewModel.fetchTemplates() } },
        )
        Box(
            modifier = Modifier
                .padding(innerPadding)
                .pullRefresh(refreshState)
        ) {
            LazyColumn(Modifier.fillMaxSize()) {
                items(viewModel.templateList, key = { it.id }) { app ->
                    TemplateItem(navigator, app)
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

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun TemplateItem(
    navigator: DestinationsNavigator,
    template: TemplateViewModel.TemplateInfo
) {
    ListItem(
        modifier = Modifier.clickable {
            navigator.navigate(TemplateEditorScreenDestination(template, !template.local))
        },
        headlineContent = { Text(template.name) },
        supportingContent = {
            Column {
                Text(template.description)
                FlowRow {
                    LabelText(label = "UID: ${template.uid}")
                    LabelText(label = "GID: ${template.gid}")
                    LabelText(label = template.context)
                    if (template.local) {
                        LabelText(label = "local")
                    } else {
                        LabelText(label = "remote")
                    }
                }
            }
        })
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit, onSync: () -> Unit, onClickCreate: () -> Unit) {
    TopAppBar(
        title = {
            Text(stringResource(R.string.settings_profile_template))
        },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
        },
        actions = {
            IconButton(onClick = onSync) {
                Icon(Icons.Filled.Sync, contentDescription = null)
            }
            IconButton(onClick = onClickCreate) {
                Icon(Icons.Filled.Create, contentDescription = null)
            }
        }
    )
}