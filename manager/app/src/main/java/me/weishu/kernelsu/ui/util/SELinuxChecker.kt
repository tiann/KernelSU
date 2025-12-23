package me.weishu.kernelsu.ui.util

import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.topjohnwu.superuser.Shell
import me.weishu.kernelsu.R

@Composable
fun getSELinuxStatus(): String {
    val shell = Shell.Builder.create().build("sh")

    val stdoutList = ArrayList<String>()
    val stderrList = ArrayList<String>()
    val result = shell.use {
        it.newJob().add("getenforce").to(stdoutList, stderrList).exec()
    }
    val stdout = stdoutList.joinToString("\n").trim()
    val stderr = stderrList.joinToString("\n").trim()

    if (result.isSuccess) {
        return when (stdout) {
            "Enforcing" -> stringResource(R.string.selinux_status_enforcing)
            "Permissive" -> stringResource(R.string.selinux_status_permissive)
            "Disabled" -> stringResource(R.string.selinux_status_disabled)
            else -> stringResource(R.string.selinux_status_unknown)
        }
    }

    return if (stderr.endsWith("Permission denied")) {
        stringResource(R.string.selinux_status_enforcing)
    } else {
        stringResource(R.string.selinux_status_unknown)
    }
}
