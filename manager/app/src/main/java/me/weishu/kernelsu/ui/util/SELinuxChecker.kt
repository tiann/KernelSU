package me.weishu.kernelsu.ui.util

import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.topjohnwu.superuser.io.SuFile
import me.weishu.kernelsu.R

@Composable
fun getSELinuxStatus() = SuFile("/sys/fs/selinux/enforce").run {
    when {
        !exists() -> stringResource(R.string.selinux_status_disabled)
        !isFile -> stringResource(R.string.selinux_status_unknown)
        !canRead() -> stringResource(R.string.selinux_status_enforcing)
        else -> when (runCatching { newInputStream() }.getOrNull()?.bufferedReader()
            ?.use { it.runCatching { readLine() }.getOrNull()?.trim()?.toIntOrNull() }) {
            1 -> stringResource(R.string.selinux_status_enforcing)
            0 -> stringResource(R.string.selinux_status_permissive)
            else -> stringResource(R.string.selinux_status_unknown)
        }
    }
}
