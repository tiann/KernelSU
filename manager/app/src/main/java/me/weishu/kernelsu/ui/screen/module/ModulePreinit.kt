package me.weishu.kernelsu.ui.screen.module

import androidx.annotation.StringRes
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.Module

/** Message for a module whose preinit actions will not run on the next boot, null otherwise. */
@StringRes
fun Module.preinitWarning(): Int? = when (preinit) {
    "no_space" -> R.string.module_preinit_no_space
    "error" -> R.string.module_preinit_error
    "bootloop" -> R.string.module_preinit_bootloop
    "unsupported" -> R.string.module_preinit_unsupported
    else -> null
}
