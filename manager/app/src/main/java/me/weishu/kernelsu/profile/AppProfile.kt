package me.weishu.kernelsu.profile

import android.os.Parcelable
import androidx.compose.runtime.Immutable
import kotlinx.parcelize.Parcelize

@Immutable
@Parcelize
data class AppProfile(
    val profileName: String,
    val allowRootRequest: Boolean = false,
    val unmountModules: Boolean = false,
) : Parcelable
