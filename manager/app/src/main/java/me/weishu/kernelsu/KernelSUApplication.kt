package me.weishu.kernelsu

import android.app.Application

lateinit var ksuApp: KernelSUApplication

class KernelSUApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        ksuApp = this
    }
}
