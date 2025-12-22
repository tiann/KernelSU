package me.weishu.kernelsu

import android.os.Parcelable
import androidx.annotation.Keep
import androidx.compose.runtime.Immutable
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable

/**
 * @author weishu
 * @date 2022/12/8.
 */
object Natives {
    // minimal supported kernel version
    // 10915: allowlist breaking change, add app profile
    // 10931: app profile struct add 'version' field
    // 10946: add capabilities
    // 10977: change groups_count and groups to avoid overflow write
    // 11071: Fix the issue of failing to set a custom SELinux type.
    // 12143: breaking: new supercall impl
    // 32243: new get_allow_list ioctl
    const val MINIMAL_SUPPORTED_KERNEL = 32243

    const val KERNEL_SU_DOMAIN = "u:r:su:s0"

    const val ROOT_UID = 0
    const val ROOT_GID = 0

    init {
        System.loadLibrary("kernelsu")
    }

    val version: Int
        external get

    val isSafeMode: Boolean
        external get

    val isLkmMode: Boolean
        external get

    val isManager: Boolean
        external get

    external fun uidShouldUmount(uid: Int): Boolean

    /**
     * Get the profile of the given package.
     * @param key usually the package name
     * @return return null if failed.
     */
    external fun getAppProfile(key: String?, uid: Int): Profile
    external fun setAppProfile(profile: Profile?): Boolean

    /**
     * `su` compat mode can be disabled temporarily.
     *  0: disabled
     *  1: enabled
     *  negative : error
     */
    external fun isSuEnabled(): Boolean
    external fun setSuEnabled(enabled: Boolean): Boolean

    /**
     * Kernel module umount can be disabled temporarily.
     *  0: disabled
     *  1: enabled
     *  negative : error
     */
    external fun isKernelUmountEnabled(): Boolean
    external fun setKernelUmountEnabled(enabled: Boolean): Boolean

    /**
     * Get the user name for the uid.
     */
    external fun getUserName(uid: Int): String?

    external fun getSuperuserCount(): Int

    private const val NON_ROOT_DEFAULT_PROFILE_KEY = "$"
    private const val NOBODY_UID = 9999

    fun setDefaultUmountModules(umountModules: Boolean): Boolean {
        Profile(
            NON_ROOT_DEFAULT_PROFILE_KEY,
            NOBODY_UID,
            false,
            umountModules = umountModules
        ).let {
            return setAppProfile(it)
        }
    }

    fun isDefaultUmountModules(): Boolean {
        getAppProfile(NON_ROOT_DEFAULT_PROFILE_KEY, NOBODY_UID).let {
            return it.umountModules
        }
    }

    fun requireNewKernel(): Boolean {
        return version != -1 && version < MINIMAL_SUPPORTED_KERNEL
    }

    @Keep
    @Immutable
    @Parcelize
    @Serializable
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
        val uid: Int = ROOT_UID,
        val gid: Int = ROOT_GID,
        val groups: List<Int> = mutableListOf(),
        val capabilities: List<Int> = mutableListOf(),
        val context: String = KERNEL_SU_DOMAIN,
        val namespace: Int = Namespace.INHERITED.ordinal,

        val nonRootUseDefault: Boolean = true,
        val umountModules: Boolean = true,
        var rules: String = "", // this field is save in ksud!!
    ) : Parcelable {
        enum class Namespace {
            INHERITED,
            GLOBAL,
            INDIVIDUAL,
        }

        constructor() : this("")
    }
}
