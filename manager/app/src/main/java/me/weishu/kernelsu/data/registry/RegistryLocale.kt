package me.weishu.kernelsu.data.registry

/**
 * Module content in the registry is single-language, so no content locale fallback exists.
 * The only locale decision left is which UI locale of the public site to link to.
 */
object RegistryLocale {

    /** UI locales of the public site (https://modules.kernelsu.org/{locale}/modules/{id}/). */
    const val SITE_LOCALE_EN = "en"
    const val SITE_LOCALE_ZH_CN = "zh-CN"

    /** Script implied by a Chinese region (CLDR likely subtags). */
    private val chineseRegionScripts = mapOf(
        "CN" to "Hans", "SG" to "Hans", "MY" to "Hans",
        "TW" to "Hant", "HK" to "Hant", "MO" to "Hant",
    )

    /**
     * Maps a device locale (BCP 47 or Android/Java `_` spelling, possibly with variants or
     * `-u-`/`-x-` extensions) to a site UI locale: Chinese written in Simplified script (zh,
     * zh-CN, zh-Hans*, zh-SG, zh-MY) -> zh-CN; everything else (incl. zh-TW/HK/MO, zh-Hant*)
     * -> en.
     */
    fun siteLocale(requested: String): String {
        val parts = requested.trim().replace('_', '-').split('-').filter { it.isNotEmpty() }
        if (parts.isEmpty() || !parts[0].equals("zh", ignoreCase = true)) return SITE_LOCALE_EN
        var i = 1
        var script: String? = null
        if (i < parts.size && parts[i].length == 4 && parts[i].all { it.isLetter() }) {
            script = parts[i].lowercase().replaceFirstChar { it.uppercaseChar() }
            i++
        }
        val region = parts.getOrNull(i)?.takeIf { it.length == 2 && it.all(Char::isLetter) }?.uppercase()
        val effective = script ?: region?.let { chineseRegionScripts[it] } ?: "Hans"
        return if (effective == "Hans") SITE_LOCALE_ZH_CN else SITE_LOCALE_EN
    }
}
