package me.weishu.kernelsu.data.model

import android.content.pm.PackageInfo
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.Natives

const val WEBVIEW_ZYGOTE_UID = 1053
const val WEBVIEW_ZYGOTE_PROFILE_KEY = "webview_zygote"

@Parcelize
data class AppInfo(
    val label: String,
    val packageInfo: PackageInfo,
    val profile: Natives.Profile?,
    val profileKey: String = packageInfo.packageName,
    val special: Boolean = false,
) : Parcelable {
    val packageName: String
        get() = packageInfo.packageName

    val displayIdentifier: String
        get() = if (special) profileKey else packageName
    val uid: Int
        get() = packageInfo.applicationInfo!!.uid

    val isWebViewZygote: Boolean
        get() = special && uid == WEBVIEW_ZYGOTE_UID

    val allowSu: Boolean
        get() = !isWebViewZygote && profile != null && profile.allowSu
    val hasCustomProfile: Boolean
        get() {
            if (profile == null) {
                return false
            }

            return if (isWebViewZygote) {
                !profile.nonRootUseDefault
            } else if (profile.allowSu) {
                !profile.rootUseDefault
            } else {
                !profile.nonRootUseDefault
            }
        }
}
