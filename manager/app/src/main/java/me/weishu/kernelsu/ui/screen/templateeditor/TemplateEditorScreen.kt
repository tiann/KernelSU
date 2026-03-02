package me.weishu.kernelsu.ui.screen.templateeditor

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode

@Composable
fun TemplateEditorScreen(template: me.weishu.kernelsu.ui.viewmodel.TemplateViewModel.TemplateInfo, readOnly: Boolean) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> TemplateEditorScreenMiuix(template, readOnly)
        UiMode.Material -> TemplateEditorScreenMaterial(template, readOnly)
    }
}
