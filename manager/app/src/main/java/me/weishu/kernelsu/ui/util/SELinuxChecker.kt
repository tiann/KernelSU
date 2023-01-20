package me.weishu.kernelsu.ui.util

import androidx.compose.ui.res.stringResource
import androidx.compose.runtime.Composable
import me.weishu.kernelsu.R

private const val TAG = "SELinuxChecker"

@Composable
fun SELinuxStatus(): String {
    val getSELinuxStatus = Runtime.getRuntime().exec("getenforce")
    getSELinuxStatus.waitFor()
    var getSELinuxStatusisSuccessful: Boolean = false
    getSELinuxStatusisSuccessful = (getSELinuxStatus.exitValue() == 0)
    val getSELinuxStatusOut = getSELinuxStatus.errorStream.bufferedReader().readLine() ?: getSELinuxStatus.inputStream.bufferedReader().readLine()
    val checkSELinuxStatus = if (getSELinuxStatusisSuccessful) {
        when (getSELinuxStatusOut) {
            "Enforcing" -> stringResource(R.string.selinux_status_enforcing)
            "Permissive" -> stringResource(R.string.selinux_status_permissive)
            "Disabled" -> stringResource(R.string.selinux_status_disabled)
            else -> stringResource(R.string.selinux_status_unknown)
        }
    } else {
        if (getSELinuxStatusOut?.endsWith("Permission denied") == true) {
            stringResource(R.string.selinux_status_enforcing)
        } else {
            stringResource(R.string.selinux_status_unknown)
        }
    }
    return checkSELinuxStatus
}
