package me.weishu.kernelsu.ui.screen.superuser

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.items
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearPrimaryButton
import me.weishu.kernelsu.ui.wear.WearSearchField
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun SuperUserPagerWear(
    uiState: SuperUserUiState,
    actions: SuperUserActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()
    var expandedUid by remember { mutableStateOf<Int?>(null) }
    val apps =
        if (uiState.searchStatus.searchText.isNotEmpty()) uiState.searchResults else uiState.groupedApps
    val stateOn = stringResource(R.string.wear_state_on)
    val stateOff = stringResource(R.string.wear_state_off)

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.superuser))
                }
            }

            item {
                WearPrimaryButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onOpenSulog,
                    label = stringResource(R.string.settings_sulog),
                )
            }

            item {
                WearSearchField(
                    value = uiState.searchStatus.searchText,
                    placeholder = stringResource(R.string.wear_search_apps),
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
                    onClick = actions.onToggleShowSystemApps,
                    label = stringResource(R.string.show_system_apps),
                    secondaryLabel = if (uiState.showSystemApps) stateOn else stateOff,
                )
            }

            if (uiState.userIds.size > 1) {
                item {
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onToggleShowOnlyPrimaryUserApps,
                        label = stringResource(R.string.show_only_primary_user_apps),
                        secondaryLabel = if (uiState.showOnlyPrimaryUserApps) stateOn else stateOff,
                    )
                }
            }

            if (uiState.isRefreshing || !uiState.hasLoaded) {
                item {
                    WearMessageText(
                        text = if (uiState.isRefreshing) stringResource(R.string.processing) else "",
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else if (apps.isEmpty()) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.wear_no_apps),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else {
                items(apps, key = { it.uid }) { group ->
                    val label = group.primary.label
                    val statusText = when {
                        group.anyAllowSu -> stringResource(R.string.superuser_allow)
                        group.anyCustom -> stringResource(R.string.profile_custom)
                        else -> stringResource(R.string.profile_default)
                    }
                    Column(
                        modifier = Modifier
                            .wearPaddedFullWidth(horizontalPadding)
                            .padding(vertical = 4.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        WearTonalButton(
                            modifier = Modifier.fillMaxWidth(),
                            onClick = {
                                expandedUid = if (expandedUid == group.uid) null else group.uid
                            },
                            label = label,
                            secondaryLabel = statusText,
                        )

                        if (expandedUid == group.uid) {
                            WearPrimaryButton(
                                modifier = Modifier.fillMaxWidth(),
                                onClick = { actions.onOpenProfile(group) },
                                label = stringResource(R.string.profile),
                            )

                            if (group.apps.size > 1) {
                                group.apps.forEach { app ->
                                    WearTonalButton(
                                        modifier = Modifier.fillMaxWidth(),
                                        onClick = { actions.onOpenProfile(group) },
                                        label = app.label,
                                        secondaryLabel = app.packageName,
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
