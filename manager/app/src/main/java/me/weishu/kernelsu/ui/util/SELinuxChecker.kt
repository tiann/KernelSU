package me.weishu.kernelsu.ui.util

import androidx.compose.ui.res.stringResource
import androidx.compose.runtime.Composable
import me.weishu.kernelsu.R

private const val TAG = "SELinuxChecker"

@Composable
fun SELinuxStatus(): String {
    val GetSELinuxStatus = Runtime.getRuntime().exec("getenforce")
    GetSELinuxStatus.waitFor()
    var GetSELinuxStatusisSuccessful: Boolean = false
    GetSELinuxStatusisSuccessful = (GetSELinuxStatus.exitValue() == 0)
    val GetSELinuxStatusOut = GetSELinuxStatus.errorStream.bufferedReader().readLine() ?: GetSELinuxStatus.inputStream.bufferedReader().readLine()
    val checkseLinuxStatus = if (GetSELinuxStatusisSuccessful) {
        when (GetSELinuxStatusOut) {
            "Enforcing" -> stringResource(R.string.selinux_status_enforcing)
            "Permissive" -> stringResource(R.string.selinux_status_permissive)
            "Disabled" -> stringResource(R.string.selinux_status_disabled)
            else -> stringResource(R.string.selinux_status_unknown)
        }
    } else {
        if (GetSELinuxStatusOut?.endsWith("Permission denied") == true) {
            stringResource(R.string.selinux_status_enforcing)
        } else {
            stringResource(R.string.selinux_status_unknown)
        }
    }
    return checkseLinuxStatus
}
