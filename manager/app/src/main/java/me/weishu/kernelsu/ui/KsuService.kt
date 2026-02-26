package me.weishu.kernelsu.ui

import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.IBinder
import android.os.UserManager
import android.util.Log
import com.topjohnwu.superuser.ipc.RootService
import me.weishu.kernelsu.IKsuInterface
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
        return try {
            // getUsers(boolean excludeDying) was added in API 17
            val method = um.javaClass.getMethod("getUsers", Boolean::class.javaPrimitiveType)
            val users = method.invoke(um, true) as List<*>

            users.map { user ->
                user!!.javaClass.getField("id").getInt(user)
            }.toIntArray()

        } catch (e: Exception) {
            Log.e(TAG, "getUsers reflection failed", e)
            intArrayOf(0)
        }
    }

    private fun getInstalledPackagesAll(flags: Int): ArrayList<PackageInfo> {
        val packages = ArrayList<PackageInfo>()
        for (userId in getAllUserIds()) {
            Log.i(TAG, "getInstalledPackagesAll: $userId")
            packages.addAll(getInstalledPackagesAsUser(flags, userId))
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
        override fun getPackages(flags: Int): ParcelableListSlice<PackageInfo> {
            val list = getInstalledPackagesAll(flags)
            Log.i(TAG, "getPackages: ${list.size}")
            return ParcelableListSlice(list)
        }

        override fun getUserIds(): IntArray {
            val ids = getAllUserIds()
            Log.i(TAG, "getUserIds: ${ids.contentToString()}")
            return ids
        }
    }
}
