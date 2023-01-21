package me.weishu.kernelsu.ui.util

import androidx.compose.ui.res.stringResource
import androidx.compose.runtime.Composable
import com.topjohnwu.superuser.Shell
import me.weishu.kernelsu.R

@Composable
fun getSELinuxStatus(): String {
    val shell = Shell.Builder.create()
        .setFlags(Shell.FLAG_REDIRECT_STDERR)
        .build("sh")

    val list = ArrayList<String>()
    val result = shell.newJob().add("getenforce").to(list, list).exec()
    val output = result.out.joinToString("\n").trim()

    if (result.isSuccess) {
        return when (output) {
            "Enforcing" -> stringResource(R.string.selinux_status_enforcing)
            "Permissive" -> stringResource(R.string.selinux_status_permissive)
            "Disabled" -> stringResource(R.string.selinux_status_disabled)
            else -> stringResource(R.string.selinux_status_unknown)
        }
    }

    return if (output.endsWith("Permission denied")) {
        stringResource(R.string.selinux_status_enforcing)
    } else {
        stringResource(R.string.selinux_status_unknown)
    }
}
