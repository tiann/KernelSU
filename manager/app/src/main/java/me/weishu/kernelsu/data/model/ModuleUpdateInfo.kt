package me.weishu.kernelsu.data.model

import androidx.compose.runtime.Immutable

@Immutable
data class ModuleUpdateInfo(
    val downloadUrl: String,
    val version: String,
    val changelog: String
) {
    companion object {
        val Empty = ModuleUpdateInfo("", "", "")
    }
}
