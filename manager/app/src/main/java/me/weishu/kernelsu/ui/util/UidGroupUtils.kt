package me.weishu.kernelsu.ui.util

import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel

object UidGroupUtils {
    private val PREFERRED_PKG_BY_SUID = mapOf(
        "android.uid.system" to "android",
        "android.uid.phone" to "com.android.phone",
        "android.uid.bluetooth" to "com.android.bluetooth",
        "android.uid.nfc" to "com.android.nfc",
    )

    fun pickPrimary(apps: List<SuperUserViewModel.AppInfo>): SuperUserViewModel.AppInfo {
        if (apps.isEmpty()) throw IllegalArgumentException("apps must not be empty")
        val bySuid = apps.groupBy { it.packageInfo.sharedUserId ?: "" }
            .filterKeys { it.startsWith("android.uid.") }
        if (bySuid.isEmpty()) return apps.first()
        val suid = bySuid.keys.minOf { it }
        val group = bySuid[suid] ?: apps
        val preferredPkg = PREFERRED_PKG_BY_SUID[suid]
        preferredPkg?.let { pkg ->
            group.firstOrNull { it.packageName == pkg }?.let { return it }
        }
        val comAndroid = group.filter { it.packageName.startsWith("com.android.") }
        if (comAndroid.isNotEmpty()) {
            return comAndroid.minWith(compareBy({ it.packageName.length }, { it.packageName }))
        }
        return group.minWith(compareBy({ it.packageName.length }, { it.packageName }))
    }

    val ownerNameCache = mutableMapOf<Int, String>()
    fun ownerNameForUid(uid: Int): String {
        ownerNameCache[uid]?.let { return it.ifEmpty { uid.toString() } }
        val name = me.weishu.kernelsu.Natives.getUserName(uid) ?: ""
        ownerNameCache[uid] = name
        return name.ifEmpty { uid.toString() }
    }


}