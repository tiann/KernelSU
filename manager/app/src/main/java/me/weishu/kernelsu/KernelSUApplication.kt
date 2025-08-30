package me.weishu.kernelsu

import android.app.Application
import android.system.Os
import com.topjohnwu.superuser.Shell
import me.weishu.kernelsu.ui.util.createRootShellBuilder
import okhttp3.Cache
import okhttp3.OkHttpClient
import java.io.File
import java.util.*

lateinit var ksuApp: KernelSUApplication

class KernelSUApplication : Application() {

    lateinit var okhttpClient: OkHttpClient

    override fun onCreate() {
        super.onCreate()
        ksuApp = this
        Shell.setDefaultBuilder(createRootShellBuilder(true))
        Shell.enableVerboseLogging = BuildConfig.DEBUG

        val webroot = File(dataDir, "webroot")
        if (!webroot.exists()) {
            webroot.mkdir()
        }

        // Provide working env for rust's temp_dir()
        Os.setenv("TMPDIR", cacheDir.absolutePath, true)

        okhttpClient =
            OkHttpClient.Builder().cache(Cache(File(cacheDir, "okhttp"), 10 * 1024 * 1024))
                .addInterceptor { block ->
                    block.proceed(
                        block.request().newBuilder()
                            .header("User-Agent", "KernelSU/${BuildConfig.VERSION_CODE}")
                            .header("Accept-Language", Locale.getDefault().toLanguageTag()).build()
                    )
                }.build()
    }
}
