package me.weishu.kernelsu

import android.app.Application
import coil.Coil
import coil.ImageLoader
import me.zhanghai.android.appiconloader.coil.AppIconFetcher
import me.zhanghai.android.appiconloader.coil.AppIconKeyer
import java.io.File

lateinit var ksuApp: KernelSUApplication

class KernelSUApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        ksuApp = this

        val context = this
        val iconSize = resources.getDimensionPixelSize(android.R.dimen.app_icon_size)
        Coil.setImageLoader(
            ImageLoader.Builder(context)
                .components {
                    add(AppIconKeyer())
                    add(AppIconFetcher.Factory(iconSize, false, context))
                }
                .build()
        )

        val webroot = File(dataDir, "webroot")
        if (!webroot.exists()) {
            webroot.mkdir()
        }
    }


}