package me.weishu.kernelsu.ui.viewmodel

import me.weishu.kernelsu.data.model.TemplateInfo

data class TemplateUiState(
    val isRefreshing: Boolean = false,
    val templates: List<TemplateInfo> = emptyList(),
    val templateList: List<TemplateInfo> = emptyList(),
    val error: Throwable? = null
)
