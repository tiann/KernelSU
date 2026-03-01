package me.weishu.kernelsu.ui.util

import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import java.util.concurrent.ConcurrentHashMap

private val PREFERRED_PKG_BY_SUID = mapOf(
    "android.uid.system" to "android",
    "android.uid.phone" to "com.android.phone",
    "android.uid.bluetooth" to "com.android.bluetooth",
    "android.uid.nfc" to "com.android.nfc",
)

fun pickPrimary(apps: List<SuperUserViewModel.AppInfo>): SuperUserViewModel.AppInfo {
    if (apps.isEmpty()) throw IllegalArgumentException("apps must not be empty")
    val labeled = apps.filter { it.packageInfo.sharedUserLabel != 0 }
    if (labeled.isNotEmpty()) {
        return labeled.minWith(compareBy({ it.packageName.length }, { it.packageName }))
    }
    val bySuid = apps.groupBy { it.packageInfo.sharedUserId ?: "" }
        .filterKeys { it.startsWith("android.uid.") }
    if (bySuid.isEmpty()) return apps.first()
    val suid = bySuid.keys.minOf { it }
    val group = bySuid[suid] ?: apps
    val preferredPkg = PREFERRED_PKG_BY_SUID[suid]
    preferredPkg?.let { pkg ->
        group.firstOrNull { it.packageName == pkg }?.let { return it }
    }
    return group.minWith(compareBy({ it.packageName.length }, { it.packageName }))
}

val ownerNameCache = ConcurrentHashMap<Int, String>()
fun ownerNameForUid(uid: Int, appSource: List<SuperUserViewModel.AppInfo>? = null): String {
    ownerNameCache[uid]?.let { return it.ifEmpty { uid.toString() } }
    val apps = (appSource ?: SuperUserViewModel.apps).filter { it.uid == uid }
    val labeledApp = apps.firstOrNull { it.packageInfo.sharedUserLabel != 0 }
    val name = if (labeledApp != null) {
        val pm = ksuApp.packageManager
        val resId = labeledApp.packageInfo.sharedUserLabel
        val text = runCatching { pm.getText(labeledApp.packageName, resId, labeledApp.packageInfo.applicationInfo) }.getOrNull()
        text?.toString() ?: ""
    } else {
        Natives.getUserName(uid) ?: ""
    }
    val appId = uid % 100000
    val isAppRange = appId in 10000..19999
    val isUA = name.matches(Regex("u\\d+_a\\d+"))
    val sharedUserId = apps.firstOrNull { !it.packageInfo.sharedUserId.isNullOrEmpty() }?.packageInfo?.sharedUserId
    val finalName = if (isAppRange && isUA && !sharedUserId.isNullOrEmpty()) {
        sharedUserId
    } else {
        name
    }
    ownerNameCache[uid] = finalName
    return finalName.ifEmpty { uid.toString() }
}
