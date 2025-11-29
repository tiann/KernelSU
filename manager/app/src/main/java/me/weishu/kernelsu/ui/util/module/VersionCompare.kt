package me.weishu.kernelsu.ui.util.module

enum class UpdateState {
    CAN_UPDATE,
    EQUAL,
    OLDER,
    NOT_INSTALLED,
    UNKNOWN
}

fun compareVersionCode(installedCode: Int?, repoCode: Int): UpdateState {
    if (repoCode <= 0) return UpdateState.UNKNOWN
    val ic = installedCode ?: return UpdateState.NOT_INSTALLED
    return when {
        repoCode > ic -> UpdateState.CAN_UPDATE
        repoCode == ic -> UpdateState.EQUAL
        else -> UpdateState.OLDER
    }
}

