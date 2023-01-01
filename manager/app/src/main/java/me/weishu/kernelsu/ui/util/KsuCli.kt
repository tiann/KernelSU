package me.weishu.kernelsu.ui.util

import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import android.util.Log
import com.topjohnwu.superuser.CallbackList
import com.topjohnwu.superuser.ShellUtils
import me.weishu.kernelsu.ksuApp
import java.io.File


/**
 * @author weishu
 * @date 2023/1/1.
 */
private const val TAG = "KsuCli"

fun execKsud(args: String): Boolean {
    val shell = ksuApp.createRootShell()
    val ksduLib = ksuApp.applicationInfo.nativeLibraryDir + File.separator + "libksud.so"
    return ShellUtils.fastCmdResult(shell, "$ksduLib $args")
}

fun toggleModule(id: String, enable: Boolean): Boolean {
    val cmd = if (enable) {
        "module enable $id"
    } else {
        "module disable $id"
    }
    val result = execKsud(cmd)
    Log.i(TAG, "toggle module $id result: $result")
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

        val shell = ksuApp.createRootShell()
        val ksduLib = ksuApp.applicationInfo.nativeLibraryDir + File.separator + "libksud.so"

        val callbackList: CallbackList<String?> = object : CallbackList<String?>() {
            override fun onAddElement(s: String?) {
                onOutput(s ?: "")
            }
        }

        val result = shell.newJob().add("$ksduLib $cmd").to(callbackList, callbackList).exec()
        Log.i("KernelSU", "install module $uri result: $result")

        file.delete()

        onFinish(result.isSuccess)
        return result.isSuccess
    }
}

fun rebootUserSpace() {
    val pm = ksuApp.getSystemService(Context.POWER_SERVICE) as PowerManager
    val shell = ksuApp.createRootShell()
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && pm.isRebootingUserspaceSupported) {
        ShellUtils.fastCmdResult(shell, "svc power reboot userspace")
    } else {
        ShellUtils.fastCmdResult(shell, "reboot")
    }
}