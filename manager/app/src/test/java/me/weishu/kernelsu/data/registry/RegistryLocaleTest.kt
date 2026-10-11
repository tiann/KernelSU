package me.weishu.kernelsu.data.registry

import org.junit.Assert.assertEquals
import org.junit.Test

class RegistryLocaleTest {

    private fun site(tag: String) = RegistryLocale.siteLocale(tag)

    @Test
    fun simplifiedChineseMapsToZhCn() {
        for (tag in listOf(
            "zh", "zh-CN", "zh_CN", "zh-Hans", "zh-Hans-CN", "zh-Hans-HK", "zh-Hans-TW", "zh-SG", "zh-MY",
            "ZH-cn", "zh-CN-u-fw-mon-mu-celsius", "zh-Hans-CN-u-nu-hanidec", "zh-u-ca-chinese",
        )) {
            assertEquals(tag, "zh-CN", site(tag))
        }
    }

    @Test
    fun traditionalChineseAndOtherLanguagesMapToEn() {
        for (tag in listOf(
            "zh-TW", "zh-HK", "zh-MO", "zh-Hant", "zh-Hant-TW", "zh-Hant-CN", "zh-HK-u-mu-celsius",
            "en", "en-US", "en-US-u-fw-mon-mu-celsius", "in-ID", "id-ID", "in", "pt-BR", "pt_BR", "ja-JP",
            "und", "", "  ", "x-private", "zhx-CN",
        )) {
            assertEquals(tag, "en", site(tag))
        }
    }
}
