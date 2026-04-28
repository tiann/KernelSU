package me.weishu.kernelsu.ui.screen.templateeditor

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.wear.WearMessageText
import me.weishu.kernelsu.ui.wear.WearPrimaryButton
import me.weishu.kernelsu.ui.wear.WearTextField
import me.weishu.kernelsu.ui.wear.WearTonalButton
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding
import me.weishu.kernelsu.ui.wear.wearPaddedFullWidth

@Composable
fun TemplateEditorScreenWear(
    state: TemplateEditorUiState,
    actions: TemplateEditorActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()
    val template = state.template

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(
                        if (state.isCreation) stringResource(R.string.app_profile_template_create)
                        else state.titleSummary
                    )
                }
            }

            item {
                WearTextField(
                    label = stringResource(R.string.app_profile_template_name),
                    value = template.name,
                    onValueChange = actions.onNameChange,
                    readOnly = state.readOnly,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                )
            }

            if (state.isCreation) {
                item {
                    WearTextField(
                        label = stringResource(R.string.app_profile_template_id),
                        value = template.id,
                        onValueChange = actions.onIdChange,
                        readOnly = state.readOnly,
                        isError = state.idErrorHint.isNotEmpty(),
                        supportingText = state.idErrorHint.takeIf { it.isNotEmpty() },
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            }

            item {
                WearTextField(
                    label = stringResource(R.string.module_author),
                    value = template.author,
                    onValueChange = actions.onAuthorChange,
                    readOnly = state.readOnly,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                )
            }

            item {
                WearTextField(
                    label = stringResource(R.string.app_profile_template_description),
                    value = template.description,
                    onValueChange = actions.onDescriptionChange,
                    readOnly = state.readOnly,
                    singleLine = false,
                    modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                )
            }

            if (state.idErrorHint.isNotEmpty()) {
                item {
                    WearMessageText(
                        text = state.idErrorHint,
                        isError = true,
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                    )
                }
            }

            if (!state.readOnly) {
                item {
                    WearPrimaryButton(
                        modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                        onClick = actions.onSave,
                        label = stringResource(R.string.save),
                    )
                }

                if (!state.isCreation) {
                    item {
                        WearTonalButton(
                            modifier = Modifier.wearPaddedFullWidth(horizontalPadding),
                            onClick = actions.onDelete,
                            label = stringResource(R.string.delete),
                        )
                    }
                }
            }
        }
    }
}
