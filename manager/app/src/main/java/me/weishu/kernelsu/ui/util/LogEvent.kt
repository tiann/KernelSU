package me.weishu.kernelsu.ui.util

import android.content.Context
import android.os.Build
import android.system.Os
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ui.screen.getManagerVersion
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
    val mountsFile = File(bugreportDir, "mounts.txt")
    val fileSystemsFile = File(bugreportDir, "filesystems.txt")
    val ksuFileTree = File(bugreportDir, "ksu_tree.txt")
    val appListFile = File(bugreportDir, "app_list.txt")
    val propFile = File(bugreportDir, "props.txt")

    val shell = createRootShell()

    shell.newJob().add("dmesg > ${dmesgFile.absolutePath}").exec()
    shell.newJob().add("logcat -d > ${logcatFile.absolutePath}").exec()
    shell.newJob().add("tar -czf ${tombstonesFile.absolutePath} /data/tombstones").exec()
    shell.newJob().add("tar -czf ${dropboxFile.absolutePath} /data/system/dropbox").exec()
    shell.newJob().add("cat /proc/mounts > ${mountsFile.absolutePath}").exec()
    shell.newJob().add("cat /proc/filesystems > ${fileSystemsFile.absolutePath}").exec()
    shell.newJob().add("ls -alRZ /data/adb > ${ksuFileTree.absolutePath}").exec()
    shell.newJob().add("cat /data/system/packages.list > ${appListFile.absolutePath}").exec()
    shell.newJob().add("getprop > ${propFile.absolutePath}").exec()

    // basic information
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
        pw.println("Manager: " + getManagerVersion(context))

        val uname = Os.uname()
        pw.println("KernelRelease: ${uname.release}")
        pw.println("KernelVersion: ${uname.version}")
        pw.println("Mahcine: ${uname.machine}")
        pw.println("Nodename: ${uname.nodename}")
        pw.println("Sysname: ${uname.sysname}")

        val ksuKernel = Natives.getVersion()
        pw.println("KernelSU: $ksuKernel")
        val safeMode = Natives.isSafeMode()
        pw.println("SafeMode: $safeMode")
    }

    // modules
    val modulesFile = File(bugreportDir, "modules.json")
    modulesFile.writeText(listModules())

    val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd_HH_mm")
    val current = LocalDateTime.now().format(formatter)

    val targetFile = File(context.cacheDir, "KernelSU_bugreport_${current}.tar.gz")

    shell.newJob().add("tar czf ${targetFile.absolutePath} -C ${bugreportDir.absolutePath} .").exec()
    shell.newJob().add("rm -rf ${bugreportDir.absolutePath}").exec()
    shell.newJob().add("chmod 0644 ${targetFile.absolutePath}").exec()

    return targetFile
}