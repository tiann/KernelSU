package me.weishu.kernelsu.ui

import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.IBinder
import android.os.UserHandle
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

    private fun getUserIds(): List<Int> {
        val result = ArrayList<Int>()
        val um = getSystemService(USER_SERVICE) as UserManager
        val userProfiles = um.userProfiles
        for (userProfile: UserHandle in userProfiles) {
            result.add(userProfile.hashCode())
        }
        return result
    }

    private fun getInstalledPackagesAll(flags: Int): ArrayList<PackageInfo> {
        val packages = ArrayList<PackageInfo>()
        for (userId in getUserIds()) {
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
    }
}