package me.weishu.kernelsu.data.model

import android.content.pm.PackageInfo
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.Natives

@Parcelize
data class AppInfo(
    val label: String,
    val packageInfo: PackageInfo,
    val profile: Natives.Profile?,
) : Parcelable {
    val packageName: String
        get() = packageInfo.packageName
    val uid: Int
        get() = packageInfo.applicationInfo!!.uid

    val allowSu: Boolean
        get() = profile != null && profile.allowSu
    val hasCustomProfile: Boolean
        get() {
            if (profile == null) {
                return false
            }

            return if (profile.allowSu) {
                !profile.rootUseDefault
            } else {
                !profile.nonRootUseDefault
            }
        }
}
