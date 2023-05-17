package me.weishu.kernelsu.profile

import android.os.Parcelable
import androidx.compose.runtime.Immutable
import kotlinx.parcelize.Parcelize

@Immutable
@Parcelize
data class RootProfile(
    val profileName: String,
    val namespace: Namespace = Namespace.Inherited,
    val uid: Int = 0,
    val gid: Int = 0,
    val groups: Int = 0,
    val capabilities: List<String> = emptyList(),
    val context: String = "u:r:su:s0",
) : Parcelable {
    enum class Namespace {
        Inherited,
        Global,
        Individual,
    }
}
