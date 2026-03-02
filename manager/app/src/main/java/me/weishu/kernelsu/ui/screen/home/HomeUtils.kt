package me.weishu.kernelsu.ui.screen.home

import android.content.Context
import android.os.Build
import android.system.Os
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.pm.PackageInfoCompat
import me.weishu.kernelsu.ui.util.getSELinuxStatus

data class ManagerVersion(
    val versionName: String,
    val versionCode: Long
)

data class SystemInfo(
    val kernelVersion: String,
    val managerVersion: String,
    val fingerprint: String,
    val selinuxStatus: String
)

@Composable
fun rememberSystemInfo(): SystemInfo {
    val context = LocalContext.current
    val uname = Os.uname()
    val managerVersion = getManagerVersion(context)
    
    return SystemInfo(
        kernelVersion = uname.release,
        managerVersion = "${managerVersion.versionName} (${managerVersion.versionCode})",
        fingerprint = Build.FINGERPRINT,
        selinuxStatus = getSELinuxStatus()
    )
}

fun getManagerVersion(context: Context): ManagerVersion {
    val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)!!
    val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo)
    return ManagerVersion(
        versionName = packageInfo.versionName!!,
        versionCode = versionCode
    )
}
