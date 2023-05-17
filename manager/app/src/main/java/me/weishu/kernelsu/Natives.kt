package me.weishu.kernelsu

import android.os.Parcelable
import androidx.compose.runtime.Immutable
import kotlinx.parcelize.Parcelize

/**
 * @author weishu
 * @date 2022/12/8.
 */
object Natives {
    const val DEFAULT_ROOT_PROFILE_KEY = "_root_default_"
    const val DEFAULT_NON_ROOT_PROFILE_KEY = "_non_root_default_"

    init {
        System.loadLibrary("kernelsu")
    }

    // become root manager, return true if success.
    external fun becomeManager(pkg: String?): Boolean
    val version: Int
        external get

    // get the uid list of allowed su processes.
    val allowList: IntArray
        external get
    val denyList: IntArray
        external get

    external fun allowRoot(uid: Int, allow: Boolean): Boolean
    val isSafeMode: Boolean
        external get

    /**
     * Get the profile of the given package.
     * @param key usually the package name
     * @return return null if failed.
     */
    external fun getAppProfile(key: String?, uid: Int): Profile
    external fun setAppProfile(profile: Profile?): Boolean


    @Immutable
    @Parcelize
    data class Profile(
        // and there is a default profile for root and non-root
        val name: String,
        // current uid for the package, this is convivent for kernel to check
        // if the package name doesn't match uid, then it should be invalidated.
        val currentUid: Int = 0,

        // if this is true, kernel will grant root permission to this package
        val allowSu: Boolean = false,

        // these are used for root profile
        val rootUseDefault: Boolean = true,
        val rootTemplate: String? = null,
        val uid: Int = 0,
        val gid: Int = 0,
        val groups: List<Int> = mutableListOf(),
        val capabilities: List<Int> = mutableListOf(),
        val context: String = "su",
        val namespace: Namespace = Namespace.Inherited,

        val nonRootUseDefault: Boolean = true,
        val umountModules: Boolean = false,
    ) : Parcelable {
        enum class Namespace {
            Inherited,
            Global,
            Individual,
        }
        constructor(): this("")
    }
}