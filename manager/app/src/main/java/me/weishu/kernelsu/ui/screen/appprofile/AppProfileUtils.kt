package me.weishu.kernelsu.ui.screen.appprofile

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R

enum class Mode(@StringRes private val res: Int) {
    Default(R.string.profile_default),
    Template(R.string.profile_template),
    Custom(R.string.profile_custom);

    val text: String
        @Composable get() = stringResource(res)
}
