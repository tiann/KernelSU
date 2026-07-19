package me.weishu.kernelsu.ui.util

import android.icu.text.Transliterator

object PinyinUtil {

    private val transliterator: Transliterator by lazy {
        Transliterator.getInstance("Han-Latin; Latin-ASCII; Lower")
    }

    @Synchronized
    fun toPinyin(input: String): String =
        transliterator.transliterate(input).replace(" ", "")
}
