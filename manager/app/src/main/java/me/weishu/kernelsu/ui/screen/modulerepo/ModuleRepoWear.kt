package me.weishu.kernelsu.ui.screen.modulerepo

import android.net.Uri
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.items
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Card
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearSearchField
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth
import java.io.File

@Composable
fun ModuleRepoScreenWear(
    uiState: ModuleRepoUiState,
    actions: ModuleRepoActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()
    val modules =
        if (uiState.searchStatus.searchText.isNotEmpty()) uiState.searchResults else uiState.modules

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.module_repos))
                }
            }

            item {
                WearSearchField(
                    value = uiState.searchStatus.searchText,
                    placeholder = stringResource(R.string.wear_search_modules),
                    onValueChange = actions.onSearchTextChange,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                )
            }

            if (uiState.searchStatus.searchText.isNotEmpty()) {
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onClearSearch,
                        label = stringResource(R.string.delete),
                    )
                }
            }

            item {
                WearTonalButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onRefresh,
                    label = stringResource(R.string.pull_down_refresh),
                )
            }

            if (uiState.isRefreshing) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.processing),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else if (modules.isEmpty()) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.module_empty),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else {
                items(modules, key = { it.moduleId }) { module ->
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = { actions.onOpenRepoDetail(module) },
                        label = module.moduleName,
                        secondaryLabel = module.authors,
                    )
                }
            }
        }
    }
}

@Composable
fun ModuleRepoDetailScreenWear(
    state: ModuleRepoDetailUiState,
    actions: ModuleRepoDetailActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(state.module.moduleName)
                }
            }

            item {
                Card(
                    onClick = {},
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                ) {
                    Text(
                        text = state.module.authors,
                        style = MaterialTheme.typography.bodySmall,
                    )
                    state.module.latestRelease.let {
                        Text(
                            text = it,
                            style = MaterialTheme.typography.labelSmall,
                        )
                    }
                }
            }

            item {
                WearTonalButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onOpenWebUrl,
                    label = stringResource(R.string.module_repo_open_web),
                )
            }

            state.detailReleases.forEach { release ->
                item {
                    Card(
                        onClick = {},
                        modifier = Modifier
                            .padding(horizontal = horizontalPadding)
                            .fillMaxWidth(),
                    ) {
                        Text(
                            text = release.name.ifEmpty { release.tagName },
                            style = MaterialTheme.typography.titleSmall,
                        )
                        release.assets.forEach { asset ->
                            var isDownloading by remember(
                                asset.name,
                                asset.downloadUrl
                            ) { mutableStateOf(false) }
                            var progress by remember(
                                asset.name,
                                asset.downloadUrl
                            ) { mutableIntStateOf(0) }
                            var downloadedUri by remember(
                                asset.name,
                                asset.downloadUrl
                            ) { mutableStateOf<Uri?>(null) }
                            WearTonalButton(
                                modifier = Modifier.fillMaxWidth(),
                                onClick = {
                                    val readyUri = downloadedUri
                                    if (readyUri != null) {
                                        actions.onInstallModule(readyUri)
                                    } else if (!isDownloading) {
                                        download(
                                            url = asset.downloadUrl,
                                            fileName = asset.name,
                                            onDownloaded = { uri ->
                                                isDownloading = false
                                                downloadedUri = uri
                                                val file = uri.path?.let(::File)
                                                if (file == null || file.exists()) {
                                                    actions.onInstallModule(uri)
                                                }
                                            },
                                            onDownloading = {
                                                isDownloading = true
                                            },
                                            onFailed = {
                                                isDownloading = false
                                            },
                                            onProgress = { p ->
                                                progress = p
                                            },
                                        )
                                    }
                                },
                                label = when {
                                    downloadedUri != null -> stringResource(R.string.install)
                                    isDownloading -> stringResource(R.string.download)
                                    else -> asset.name
                                },
                                secondaryLabel = when {
                                    downloadedUri != null -> asset.name
                                    isDownloading -> "${progress}%"
                                    else -> null
                                },
                            )
                        }
                    }
                }
            }
        }
    }
}
