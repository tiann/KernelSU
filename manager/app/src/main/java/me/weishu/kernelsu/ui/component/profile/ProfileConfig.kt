package me.weishu.kernelsu.ui.component.profile

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import me.weishu.kernelsu.Natives

@Composable
fun AppProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    enabled: Boolean,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    AppProfileConfigMaterial(
        modifier = modifier,
        fixedName = fixedName,
        enabled = enabled,
        profile = profile,
        onProfileChange = onProfileChange
    )
}

@Composable
fun RootProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    enabled: Boolean = true,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    RootProfileConfigMaterial(
        modifier = modifier,
        enabled = enabled,
        profile = profile,
        onProfileChange = onProfileChange
    )
}

@Composable
fun TemplateConfig(
    modifier: Modifier = Modifier,
    profile: Natives.Profile,
    onViewTemplate: (id: String) -> Unit = {},
    onManageTemplate: () -> Unit = {},
    onProfileChange: (Natives.Profile) -> Unit
) {
    TemplateConfigMaterial(
        profile = profile,
        onViewTemplate = onViewTemplate,
        onManageTemplate = onManageTemplate,
        onProfileChange = onProfileChange
    )
}
