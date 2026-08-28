package me.weishu.kernelsu.ui.util

import android.annotation.SuppressLint
import android.os.Build
import java.io.File

private const val SAMSUNG_FLOATING_FEATURE_CLASS = "com.samsung.android.feature.SemFloatingFeature"
private const val KEY_SAMSUNG_PRODUCT_NAME = "SEC_FLOATING_FEATURE_SETTINGS_CONFIG_BRAND_NAME"

private val SAMSUNG_FLOATING_FEATURE_PATHS = listOf(
    "/vendor/etc/floating_feature.xml",
    "/system/etc/floating_feature.xml",
)

/**
 * Resolve the commercial device name.
 * 1. Dispatch by [Build.MANUFACTURER] — each vendor branch uses its own property
 *    or dedicated helper (e.g. Samsung).
 * 2. If the vendor branch returns nothing, Fallback to MANUFACTURER + BRAND + MODEL.
 */
fun resolveDeviceName(): String {
    val marketName = when (Build.MANUFACTURER.orEmpty().lowercase()) {
        "samsung" -> getSamsungProductName()
        "xiaomi" -> getSystemProperty("ro.product.marketname").takeIfValidDeviceName()
        "oppo", "oneplus", "realme", "oplus" ->
            getSystemProperty("ro.vendor.oplus.market.name").takeIfValidDeviceName()

        "vivo" -> getSystemProperty("ro.vivo.market.name").takeIfValidDeviceName()
        "honor", "huawei" -> getSystemProperty("ro.config.marketing_name").takeIfValidDeviceName()
        "zte", "nubia" -> getSystemProperty("ro.vendor.product.ztename").takeIfValidDeviceName()

        else -> null
    }
    if (marketName != null) return marketName

    return buildString {
        append(Build.MANUFACTURER)
        if (!Build.BRAND.equals(Build.MANUFACTURER, ignoreCase = true)) append(" ").append(Build.BRAND)
        append(" ").append(Build.MODEL)
    }
}

private fun getSamsungProductName(): String? {
    val name = getSamsungProductNameFromFloatingFeature()
        ?: getSamsungProductNameFromFloatingFeatureFile()
    return name.withSamsungPrefix()
}

@SuppressLint("PrivateApi")
private fun getSamsungProductNameFromFloatingFeature(): String? = try {
    val clazz = Class.forName(SAMSUNG_FLOATING_FEATURE_CLASS)
    val instance = clazz.getMethod("getInstance").invoke(null)
    val value = clazz.getMethod("getString", String::class.java)
        .invoke(instance, KEY_SAMSUNG_PRODUCT_NAME) as? String
    value.takeIfValidDeviceName()
} catch (_: Throwable) {
    null
}

private fun getSamsungProductNameFromFloatingFeatureFile(): String? {
    val pattern = Regex(
        "<$KEY_SAMSUNG_PRODUCT_NAME>\\s*(.*?)\\s*</$KEY_SAMSUNG_PRODUCT_NAME>",
        RegexOption.DOT_MATCHES_ALL,
    )
    return SAMSUNG_FLOATING_FEATURE_PATHS.firstNotNullOfOrNull { path ->
        try {
            pattern.find(File(path).readText())?.groupValues?.getOrNull(1).takeIfValidDeviceName()
        } catch (_: Throwable) {
            null
        }
    }
}

private fun String?.takeIfValidDeviceName(): String? =
    this?.trim()?.takeIf {
        it.isNotEmpty() &&
                !it.equals(Build.UNKNOWN, ignoreCase = true) &&
                !it.equals("null", ignoreCase = true)
    }

private fun String?.withSamsungPrefix(): String? {
    val name = this.takeIfValidDeviceName() ?: return null
    return if (name.startsWith("Samsung", ignoreCase = true)) name else "Samsung $name"
}
