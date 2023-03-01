package me.weishu.kernelsu.ui.util

import android.net.Uri
import android.os.SystemClock
import android.util.Log
import com.topjohnwu.superuser.CallbackList
import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.ShellUtils
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.ksuApp
import java.io.File


/**
 * @author weishu
 * @date 2023/1/1.
 */
private const val TAG = "KsuCli"

private fun getKsuDaemonPath(): String {
    return ksuApp.applicationInfo.nativeLibraryDir + File.separator + "libksud.so"
}

fun createRootShell(): Shell {
    Shell.enableVerboseLogging = BuildConfig.DEBUG
    val builder = Shell.Builder.create()
    return try {
        builder.build(getKsuDaemonPath(), "debug", "su")
    } catch (e: Throwable) {
        Log.e(TAG, "su failed: ", e)
        builder.build("sh")
    }
}

fun execKsud(args: String): Boolean {
    val shell = createRootShell()
    return ShellUtils.fastCmdResult(shell, "${getKsuDaemonPath()} $args")
}

fun install() {
    val start = SystemClock.elapsedRealtime()
    val result = execKsud("install")
    Log.w(TAG, "install result: $result, cost: ${SystemClock.elapsedRealtime() - start}ms")
}

fun listModules(): String {
    val shell = createRootShell()

    val out = shell.newJob().add("${getKsuDaemonPath()} module list").to(ArrayList(), null).exec().out
    return out.joinToString("\n").ifBlank { "[]" }
}

fun toggleModule(id: String, enable: Boolean): Boolean {
    val cmd = if (enable) {
        "module enable $id"
    } else {
        "module disable $id"
    }
    val result = execKsud(cmd)
    Log.i(TAG, "$cmd result: $result")
    return result
}

fun uninstallModule(id: String) : Boolean {
    val cmd = "module uninstall $id"
    val result = execKsud(cmd)
    Log.i(TAG, "uninstall module $id result: $result")
    return result
}

fun installModule(uri: Uri, onFinish: (Boolean)->Unit, onOutput: (String) -> Unit) : Boolean {
    val resolver = ksuApp.contentResolver
    with(resolver.openInputStream(uri)) {
        val file = File(ksuApp.cacheDir, "module.zip")
        file.outputStream().use { output ->
            this?.copyTo(output)
        }
        val cmd = "module install ${file.absolutePath}"

        val shell = createRootShell()

        val callbackList: CallbackList<String?> = object : CallbackList<String?>() {
            override fun onAddElement(s: String?) {
                onOutput(s ?: "")
            }
        }

        val result = shell.newJob().add("${getKsuDaemonPath()} $cmd").to(callbackList, callbackList).exec()
        Log.i("KernelSU", "install module $uri result: $result")

        file.delete()

        onFinish(result.isSuccess)
        return result.isSuccess
    }
}

fun reboot(reason: String = "") {
    val shell = createRootShell()
    if (reason == "recovery") {
        // KEYCODE_POWER = 26, hide incorrect "Factory data reset" message
        ShellUtils.fastCmd(shell, "/system/bin/input keyevent 26")
    }
    ShellUtils.fastCmd(shell, "/system/bin/svc power reboot $reason || /system/bin/reboot $reason")
}

fun overlayFsAvailable(): Boolean {
    val shell = createRootShell()
    // check /proc/filesystems
    return ShellUtils.fastCmdResult(shell, "cat /proc/filesystems | grep overlay")
}

fun hasMagisk(): Boolean {
    val shell = createRootShell()
    val result = shell.newJob().add("nsenter --mount=/proc/1/ns/mnt which magisk").exec()
    Log.i(TAG, "has magisk: ${result.isSuccess}")
    return result.isSuccess
}