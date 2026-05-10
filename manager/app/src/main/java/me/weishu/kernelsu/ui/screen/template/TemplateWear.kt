package me.weishu.kernelsu.ui.screen.template

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.items
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearPrimaryButton
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun AppProfileTemplateScreenWear(
    state: TemplateUiState,
    actions: TemplateActions,
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
                    Text(stringResource(R.string.settings_profile_template))
                }
            }

            item {
                WearPrimaryButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = actions.onCreateTemplate,
                    label = stringResource(R.string.app_profile_template_create),
                )
            }

            item {
                WearTonalButton(
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    onClick = { actions.onRefresh(true) },
                    label = stringResource(R.string.pull_down_refresh),
                )
            }

            if (state.isRefreshing) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.processing),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else if (state.templateList.isEmpty()) {
                item {
                    WearMessageText(
                        text = stringResource(R.string.app_profile_template_empty),
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            } else {
                items(state.templateList, key = { it.id }) { template ->
                    WearTonalButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = { actions.onOpenTemplate(template) },
                        label = template.name,
                        secondaryLabel = template.author,
                    )
                }
            }
        }
    }
}
