package me.weishu.kernelsu.ui.util

import android.content.Context
import android.os.Build
import android.system.Os
import java.io.File
import java.io.FileWriter
import java.io.PrintWriter
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

fun getBugreportFile(context: Context): File {

    val bugreportDir = File(context.cacheDir, "bugreport")
    bugreportDir.mkdirs()

    val dmesgFile = File(bugreportDir, "dmesg.txt")
    val logcatFile = File(bugreportDir, "logcat.txt")
    val tombstonesFile = File(bugreportDir, "tombstones.tar.gz")
    val dropboxFile = File(bugreportDir, "dropbox.tar.gz")

    val shell = createRootShell()

    shell.newJob().add("dmesg > ${dmesgFile.absolutePath}").exec()
    shell.newJob().add("logcat -d > ${logcatFile.absolutePath}").exec()
    shell.newJob().add("tar -czf ${tombstonesFile.absolutePath} /data/tombstones").exec()
    shell.newJob().add("tar -czf ${dropboxFile.absolutePath} /data/system/dropbox").exec()

    // build.prop
    val buildInfo = File(bugreportDir, "basic.txt")
    PrintWriter(FileWriter(buildInfo)).use { pw ->
        pw.println("Kernel: ${System.getProperty("os.version")}")
        pw.println("BRAND: " + Build.BRAND)
        pw.println("MODEL: " + Build.MODEL)
        pw.println("PRODUCT: " + Build.PRODUCT)
        pw.println("MANUFACTURER: " + Build.MANUFACTURER)
        pw.println("SDK: " + Build.VERSION.SDK_INT)
        pw.println("PREVIEW_SDK: " + Build.VERSION.PREVIEW_SDK_INT)
        pw.println("FINGERPRINT: " + Build.FINGERPRINT)
        pw.println("DEVICE: " + Build.DEVICE)
        val packageInfo =
            context.packageManager.getPackageInfo(context.packageName, 0)
        pw.println("PACKAGE: " + packageInfo.packageName)
        pw.println("VERSION: " + packageInfo.versionName)

        val uname = Os.uname()
        pw.println("KernelRelease: ${uname.release}")
        pw.println("KernelVersion: ${uname.version}")
        pw.println("Mahcine: ${uname.machine}")
        pw.println("Nodename: ${uname.nodename}")
        pw.println("Sysname: ${uname.sysname}")

    }

    // modules
    val modulesFile = File(bugreportDir, "modules.json")
    modulesFile.writeText(listModules())

    val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd_HH_mm")
    val current = LocalDateTime.now().format(formatter)

    val targetFile = File(context.cacheDir, "KernelSU_bugreport_${current}.tar.gz")

    shell.newJob().add("tar czf ${targetFile.absolutePath}  -C ${bugreportDir.absolutePath} .").exec()
    shell.newJob().add("rm -rf ${bugreportDir.absolutePath}").exec()
    shell.newJob().add("chmod 0644 ${targetFile.absolutePath}").exec()

    return targetFile;
}