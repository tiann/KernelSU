package me.weishu.kernelsu.ui.component.profile

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Create
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.listAppProfileTemplates
import me.weishu.kernelsu.ui.util.setSepolicy
import me.weishu.kernelsu.ui.viewmodel.getTemplateInfoById
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.extra.DropDownMode
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperDropdown
import top.yukonga.miuix.kmp.theme.MiuixTheme

/**
 * @author weishu
 * @date 2023/10/21.
 */
@Composable
fun TemplateConfig(
    profile: Natives.Profile,
    onViewTemplate: (id: String) -> Unit = {},
    onManageTemplate: () -> Unit = {},
    onProfileChange: (Natives.Profile) -> Unit
) {
    var expanded by remember { mutableStateOf(false) }
    var template by rememberSaveable {
        mutableStateOf(profile.rootTemplate ?: "")
    }
    val profileTemplates = listAppProfileTemplates()
    val noTemplates = profileTemplates.isEmpty()

    if (noTemplates) {
        SuperArrow(
            title = stringResource(R.string.app_profile_template_create),
            leftAction = {
                Icon(
                    Icons.Rounded.Create,
                    null,
                    modifier = Modifier.padding(end = 16.dp),
                    tint = MiuixTheme.colorScheme.onBackground
                )
            },
            onClick = onManageTemplate,
        )
    } else {
        Column {
            SuperDropdown(
                title = stringResource(R.string.profile_template),
                items = profileTemplates,
                selectedIndex = profileTemplates.indexOf(template).takeIf { it >= 0 } ?: 0,
                onSelectedIndexChange = { index ->
                    if (index < 0 || index >= profileTemplates.size) return@SuperDropdown
                    template = profileTemplates[index]
                    val templateInfo = getTemplateInfoById(template)
                    if (templateInfo != null && setSepolicy(template, templateInfo.rules.joinToString("\n"))) {
                        onProfileChange(
                            profile.copy(
                                rootTemplate = template,
                                rootUseDefault = false,
                                uid = templateInfo.uid,
                                gid = templateInfo.gid,
                                groups = templateInfo.groups,
                                capabilities = templateInfo.capabilities,
                                context = templateInfo.context,
                                namespace = templateInfo.namespace,
                            )
                        )
                    }
                },
                onClick = {
                    expanded = !expanded
                },
                mode = DropDownMode.AlwaysOnRight,
                maxHeight = 280.dp
            )
            SuperArrow(
                title = stringResource(R.string.app_profile_template_view),
                onClick = { onViewTemplate(template) }
            )
        }
    }
}