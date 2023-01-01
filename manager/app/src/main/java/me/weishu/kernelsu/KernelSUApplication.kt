package me.weishu.kernelsu

import android.app.Application
import android.util.Log
import coil.Coil
import coil.ImageLoader
import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.ShellUtils
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

        install()
    }

    fun createRootShell(): Shell {
        Shell.enableVerboseLogging = BuildConfig.DEBUG
        val su = applicationInfo.nativeLibraryDir + File.separator + "libksu.so"
        val builder = Shell.Builder.create()
        return builder.build(su)
    }

    fun install() {
        val shell = createRootShell()
        val ksduLib = ksuApp.applicationInfo.nativeLibraryDir + File.separator + "libksud.so"
        val result = ShellUtils.fastCmdResult(shell, "$ksduLib install")
        Log.w("KernelSU", "install ksud result: $result")
    }
}