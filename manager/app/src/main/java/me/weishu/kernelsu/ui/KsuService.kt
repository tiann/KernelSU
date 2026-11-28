package me.weishu.kernelsu.ui

import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.IBinder
import android.os.UserManager
import android.util.Log
import com.topjohnwu.superuser.ipc.RootService
import me.weishu.kernelsu.IKsuInterface
import me.weishu.kernelsu.data.model.HYPEROS_PACKAGE
import me.weishu.kernelsu.data.model.WEBVIEW_ZYGOTE_UID
import me.weishu.kernelsu.ui.util.isHyperOS
import rikka.parcelablelist.ParcelableListSlice

/**
 * @author weishu
 * @date 2023/4/18.
 */

class KsuService : RootService() {

    companion object {
        private const val TAG = "KsuService"
    }

    override fun onBind(intent: Intent): IBinder {
        return Stub()
    }

    private fun getAllUserIds(): IntArray {
        val um = getSystemService(USER_SERVICE) as UserManager
        // getAliveUsers() was added in API 31
        try {
            val method = um.javaClass.getMethod("getAliveUsers")
            val users = method.invoke(um) as List<*>
            return extractUserIds(users)
        } catch (e: Exception) {
            Log.e(TAG, "getAliveUsers reflection failed", e)
        }

        return intArrayOf(0)
    }

    private fun extractUserIds(users: List<*>?): IntArray {
        if (users.isNullOrEmpty()) return intArrayOf(0)

        return try {
            users.map { user ->
                user!!.javaClass.getField("id").getInt(user)
            }.toIntArray()
        } catch (e: Exception) {
            Log.e(TAG, "Error extracting ID from UserInfo", e)
            intArrayOf(0)
        }
    }

    private fun loadPackagesForAppList(readHyperOsMetadata: Boolean): ArrayList<PackageInfo> {
        val packages = ArrayList<PackageInfo>()
        val hyperOs = isHyperOS()
        val flags = if (hyperOs && readHyperOsMetadata) PackageManager.GET_META_DATA else 0
        for (userId in getAllUserIds()) {
            Log.i(TAG, "Loading app list for user $userId")
            getInstalledPackagesAsUser(flags, userId).filterTo(packages) { pkg ->
                val app = pkg.applicationInfo ?: return@filterTo false
                val hasCode = (app.flags and ApplicationInfo.FLAG_HAS_CODE) != 0
                if (flags == PackageManager.GET_META_DATA) {
                    // Only this flag is needed by the Manager. Keep other metadata out of Binder.
                    app.metaData = if (!hasCode && app.metaData?.getBoolean(HYPEROS_PACKAGE) == true) {
                        Bundle(1).apply { putBoolean(HYPEROS_PACKAGE, true) }
                    } else null
                }
                app.uid != WEBVIEW_ZYGOTE_UID && (hasCode || hyperOs)
            }
        }
        return packages
    }

    @Suppress("UNCHECKED_CAST")
    private fun getInstalledPackagesAsUser(flags: Int, userId: Int): List<PackageInfo> {
        return try {
            val pm: PackageManager = packageManager
            val method = pm.javaClass.getDeclaredMethod(
                "getInstalledPackagesAsUser",
                Int::class.javaPrimitiveType,
                Int::class.javaPrimitiveType
            )
            method.invoke(pm, flags, userId) as List<PackageInfo>
        } catch (e: Throwable) {
            Log.e(TAG, "err", e)
            ArrayList()
        }
    }

    private inner class Stub : IKsuInterface.Stub() {
        override fun getPackagesForAppList(readHyperOsMetadata: Boolean): ParcelableListSlice<PackageInfo> {
            val list = loadPackagesForAppList(readHyperOsMetadata)
            Log.i(TAG, "getPackagesForAppList: ${list.size}")
            return ParcelableListSlice(list)
        }

        override fun getUserIds(): IntArray {
            val ids = getAllUserIds()
            Log.i(TAG, "getUserIds: ${ids.contentToString()}")
            return ids
        }
    }
}
