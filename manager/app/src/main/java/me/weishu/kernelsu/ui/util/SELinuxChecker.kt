package me.weishu.kernelsu.ui.util

import com.topjohnwu.superuser.Shell

fun isSELinuxPermissive(): Boolean {
    val shell = Shell.Builder.create().build("sh")
    val stdoutList = ArrayList<String>()
    val result = shell.use {
        it.newJob().add("getenforce").to(stdoutList).exec()
    }
    return result.isSuccess && stdoutList.joinToString("").trim() == "Permissive"
}

/**
 * Returns the raw SELinux status string ("Enforcing", "Permissive", "Disabled", or "Unknown").
 * Safe to call from any thread (IO recommended).
 */
fun getSELinuxStatusRaw(): String {
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
            "Enforcing", "Permissive", "Disabled" -> stdout
            else -> "Unknown"
        }
    }

    return if (stderr.endsWith("Permission denied")) {
        "Enforcing"
    } else {
        "Unknown"
    }
}
