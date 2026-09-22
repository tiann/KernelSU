package me.weishu.kernelsu

import android.system.Os

/**
 * @author weishu
 * @date 2022/12/10.
 */

data class KernelVersion(val major: Int, val patchLevel: Int, val subLevel: Int) {
    override fun toString(): String {
        return "$major.$patchLevel.$subLevel"
    }

    fun isGKI(): Boolean {

        // kernel 6.x
        if (major > 5) {
            return true
        }

        // kernel 5.10.x
        if (major == 5) {
            return patchLevel >= 10
        }

        return false
    }
}

fun parseKernelVersion(version: String): KernelVersion {
    val find = "(\\d+)\\.(\\d+)\\.(\\d+)".toRegex().find(version)
    if (find != null) {
        val major = find.groupValues[1].toIntOrNull()
        val patchLevel = find.groupValues[2].toIntOrNull()
        val subLevel = find.groupValues[3].toIntOrNull()
        if (major != null && patchLevel != null && subLevel != null) {
            return KernelVersion(major, patchLevel, subLevel)
        }
    }
    return KernelVersion(-1, -1, -1)
}

fun getKernelVersion(): KernelVersion {
    Os.uname().release.let {
        return parseKernelVersion(it)
    }
}