package me.weishu.kernelsu.ui.screen.templateeditor

import android.widget.Toast
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.DeleteForever
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.dropUnlessResumed
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedTextField
import me.weishu.kernelsu.ui.component.profile.RootProfileConfig
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.util.deleteAppProfileTemplate
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel

@OptIn(ExperimentalComposeUiApi::class, ExperimentalMaterial3Api::class)
@Composable
fun TemplateEditorScreenMaterial(
    initialTemplate: TemplateViewModel.TemplateInfo,
    readOnly: Boolean = true,
) {
    val navigator = LocalNavigator.current
    val isCreation = initialTemplate.id.isBlank()

    var template by rememberSaveable {
        mutableStateOf(initialTemplate)
    }

    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())

    LaunchedEffect(Unit) {
        scrollBehavior.state.heightOffset = scrollBehavior.state.heightOffsetLimit
    }

    Scaffold(
        topBar = {
            val author = if (initialTemplate.author.isNotEmpty()) "@${initialTemplate.author}" else ""

            val titleSummary = initialTemplate.id + author
            val saveTemplateFailed = stringResource(id = R.string.app_profile_template_save_failed)
            val context = LocalContext.current

            TopBar(
                title = if (isCreation) {
                    stringResource(R.string.app_profile_template_create)
                } else if (readOnly) {
                    stringResource(R.string.app_profile_template_view)
                } else {
                    stringResource(R.string.app_profile_template_edit)
                },
                readOnly = readOnly,
                summary = titleSummary,
                onBack = dropUnlessResumed {
                    if (!readOnly) {
                        navigator.setResult("template_edit", true)
                    } else {
                        navigator.pop()
                    }
                },
                onDelete = {
                    if (deleteAppProfileTemplate(template.id)) {
                        navigator.setResult("template_edit", true)
                    }
                },
                onSave = {
                    if (saveTemplate(template, isCreation)) {
                        navigator.setResult("template_edit", true)
                    } else {
                        Toast.makeText(context, saveTemplateFailed, Toast.LENGTH_SHORT).show()
                    }
                },
                scrollBehavior = scrollBehavior
            )
        },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .imePadding()
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState())
        ) {
            var idErrorHint by remember { mutableStateOf("") }
            val idConflictError = stringResource(id = R.string.app_profile_template_id_exist)
            val idInvalidError = stringResource(id = R.string.app_profile_template_id_invalid)

            SegmentedColumn(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                content = buildList {
                    add(
                        {
                            TemplateEditorListItem(
                                label = stringResource(id = R.string.app_profile_template_name),
                                value = template.name,
                                readOnly = readOnly,
                                onValueChange = { value ->
                                    template = template.copy(name = value)
                                }
                            )
                        }
                    )
                    if (isCreation) {
                        add(
                            {
                                TemplateEditorListItem(
                                    label = stringResource(id = R.string.app_profile_template_id),
                                    value = template.id,
                                    errorHint = idErrorHint,
                                    isError = idErrorHint.isNotEmpty(),
                                    readOnly = readOnly,
                                    onValueChange = { value ->
                                        idErrorHint = if (isTemplateExist(value)) {
                                            idConflictError
                                        } else if (!isValidTemplateId(value)) {
                                            idInvalidError
                                        } else {
                                            ""
                                        }
                                        template = template.copy(id = value)
                                    }
                                )
                            }
                        )
                    }
                    add(
                        {
                            TemplateEditorListItem(
                                label = stringResource(id = R.string.module_author),
                                value = template.author,
                                readOnly = readOnly,
                                onValueChange = { value ->
                                    template = template.copy(author = value)
                                }
                            )
                        }
                    )
                    add(
                        {
                            TemplateEditorListItem(
                                label = stringResource(id = R.string.app_profile_template_description),
                                value = template.description,
                                multiline = true,
                                readOnly = readOnly,
                                onValueChange = { value ->
                                    template = template.copy(description = value)
                                }
                            )
                        }
                    )
                }
            )

            RootProfileConfig(
                fixedName = true,
                enabled = !readOnly,
                profile = toNativeProfile(template),
                onProfileChange = {
                    template.copy(
                        uid = it.uid,
                        gid = it.gid,
                        groups = it.groups,
                        capabilities = it.capabilities,
                        context = it.context,
                        namespace = it.namespace,
                        rules = it.rules.split("\n")
                    ).run {
                        template = this
                    }
                }
            )

            Spacer(
                Modifier.height(
                    WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding()
                )
            )
        }
    }
}

@Composable
private fun TemplateEditorListItem(
    label: String,
    value: String,
    errorHint: String = "",
    isError: Boolean = false,
    multiline: Boolean = false,
    readOnly: Boolean = false,
    onValueChange: (String) -> Unit
) {
    SegmentedTextField(
        value = value,
        onValueChange = onValueChange,
        label = label,
        supportingContent = if (isError && errorHint.isNotEmpty()) {
            { Text(errorHint, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.labelSmall) }
        } else null,
        isError = isError,
        singleLine = !multiline,
        minLines = 1,
        maxLines = if (multiline) 100 else 1,
        readOnly = readOnly
    )
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun TopBar(
    title: String,
    readOnly: Boolean,
    summary: String = "",
    onBack: () -> Unit,
    onDelete: () -> Unit = {},
    onSave: () -> Unit = {},
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    LargeFlexibleTopAppBar(
        title = {
            Column {
                Text(title)
                if (summary.isNotBlank()) {
                    Text(
                        text = summary,
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
        },
        navigationIcon = {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null)
            }
        },
        actions = {
            if (readOnly) return@LargeFlexibleTopAppBar
            IconButton(onClick = onDelete) {
                Icon(
                    Icons.Filled.DeleteForever,
                    contentDescription = stringResource(id = R.string.app_profile_template_delete)
                )
            }
            IconButton(onClick = onSave) {
                Icon(
                    imageVector = Icons.Filled.Save,
                    contentDescription = stringResource(id = R.string.app_profile_template_save)
                )
            }
        },
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface,
            scrolledContainerColor = MaterialTheme.colorScheme.surface
        ),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}
