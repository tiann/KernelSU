package me.weishu.kernelsu.ui.util

import android.annotation.SuppressLint

@SuppressLint("PrivateApi")
private fun getSystemProperty(key: String): String {
    return try {
        val props = Class.forName("android.os.SystemProperties")
        props.getMethod("get", String::class.java).invoke(null, key) as? String ?: ""
    } catch (_: Throwable) {
        ""
    }
}

fun isMiui(): Boolean {
    return getSystemProperty("ro.miui.ui.version.name").isNotEmpty()
}

fun isHyperOS(): Boolean {
    return getSystemProperty("ro.mi.os.version.name").isNotEmpty()
}

fun isColorOS(): Boolean {
    return getSystemProperty("ro.build.version.oplus.api").isNotEmpty() || getSystemProperty("ro.vendor.oplus.market.name").isNotEmpty()
}
