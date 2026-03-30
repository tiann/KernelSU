package me.weishu.kernelsu.ui.util

import java.io.File

/**
 * Returns the raw Seccomp status string:
 * - "NotSupported": kernel compiled without CONFIG_SECCOMP
 * - "Disabled": kernel supports seccomp but no filter applied to apps
 * - "Enabled": seccomp filters are active (strict or filter mode)
 */
fun getSeccompStatusRaw(): String {
    return try {
        val statusContent = File("/proc/self/status").readText()
        val seccompLine = statusContent.lines().find { it.startsWith("Seccomp:") }
        if (seccompLine == null) {
            "NotSupported"
        } else {
            when (seccompLine.substringAfter(":").trim()) {
                "0" -> "Disabled"
                "1", "2" -> "Enabled"
                else -> "NotSupported"
            }
        }
    } catch (_: Exception) {
        "NotSupported"
    }
}
