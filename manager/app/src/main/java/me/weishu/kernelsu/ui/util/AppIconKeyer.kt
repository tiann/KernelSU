package me.weishu.kernelsu.ui.util

import android.content.pm.PackageInfo
import coil3.key.Keyer
import coil3.request.Options
import me.zhanghai.android.appiconloader.AppIconLoader

class AppIconKeyer : Keyer<PackageInfo> {
    override fun key(
        data: PackageInfo,
        options: Options
    ): String {
        return AppIconLoader.getIconKey(data, options.context)
    }
}