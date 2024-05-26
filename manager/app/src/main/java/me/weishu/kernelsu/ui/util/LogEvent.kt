package me.weishu.kernelsu.ui.util

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.ParcelFileDescriptor
import android.system.Os
import com.topjohnwu.superuser.ShellUtils
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ui.screen.getManagerVersion
import java.io.File
import java.io.FileOutputStream
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
    val pstoreFile = File(bugreportDir, "pstore.tar.gz")
    // Xiaomi/Readmi devices have diag in /data/vendor/diag
    val diagFile = File(bugreportDir, "diag.tar.gz")
    val opulsFile = File(bugreportDir, "opuls.tar.gz")
    val bootlogFile = File(bugreportDir, "bootlog.tar.gz")
    val mountsFile = File(bugreportDir, "mounts.txt")
    val fileSystemsFile = File(bugreportDir, "filesystems.txt")
    val adbFileTree = File(bugreportDir, "adb_tree.txt")
    val adbFileDetails = File(bugreportDir, "adb_details.txt")
    val ksuFileSize = File(bugreportDir, "ksu_size.txt")
    val appListFile = File(bugreportDir, "packages.txt")
    val propFile = File(bugreportDir, "props.txt")
    val allowListFile = File(bugreportDir, "allowlist.bin")
    val procModules = File(bugreportDir, "proc_modules.txt")
    val bootConfig = File(bugreportDir, "boot_config.txt")
    val kernelConfig = File(bugreportDir, "defconfig.gz")

    val shell = getRootShell(true)

    shell.newJob().add("dmesg > ${dmesgFile.absolutePath}").exec()
    shell.newJob().add("logcat -d > ${logcatFile.absolutePath}").exec()
    shell.newJob().add("tar -czf ${tombstonesFile.absolutePath} -C /data/tombstones .").exec()
    shell.newJob().add("tar -czf ${dropboxFile.absolutePath} -C /data/system/dropbox .").exec()
    shell.newJob().add("tar -czf ${pstoreFile.absolutePath} -C /sys/fs/pstore .").exec()
    shell.newJob().add("tar -czf ${diagFile.absolutePath} -C /data/vendor/diag . --exclude=./minidump.gz").exec()
    shell.newJob().add("tar -czf ${opulsFile.absolutePath} -C /mnt/oplus/op2/media/log/boot_log/ .").exec()
    shell.newJob().add("tar -czf ${bootlogFile.absolutePath} -C /data/adb/ksu/log .").exec()

    shell.newJob().add("cat /proc/1/mountinfo > ${mountsFile.absolutePath}").exec()
    shell.newJob().add("cat /proc/filesystems > ${fileSystemsFile.absolutePath}").exec()
    shell.newJob().add("busybox tree /data/adb > ${adbFileTree.absolutePath}").exec()
    shell.newJob().add("ls -alRZ /data/adb > ${adbFileDetails.absolutePath}").exec()
    shell.newJob().add("du -sh /data/adb/ksu/* > ${ksuFileSize.absolutePath}").exec()
    shell.newJob().add("cp /data/system/packages.list ${appListFile.absolutePath}").exec()
    shell.newJob().add("getprop > ${propFile.absolutePath}").exec()
    shell.newJob().add("cp /data/adb/ksu/.allowlist ${allowListFile.absolutePath}").exec()
    shell.newJob().add("cp /proc/modules ${procModules.absolutePath}").exec()
    shell.newJob().add("cp /proc/bootconfig ${bootConfig.absolutePath}").exec()
    shell.newJob().add("cp /proc/config.gz ${kernelConfig.absolutePath}").exec()

    val selinux = ShellUtils.fastCmd(shell, "getenforce")

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
        pw.println("SELinux: $selinux")

        val uname = Os.uname()
        pw.println("KernelRelease: ${uname.release}")
        pw.println("KernelVersion: ${uname.version}")
        pw.println("Machine: ${uname.machine}")
        pw.println("Nodename: ${uname.nodename}")
        pw.println("Sysname: ${uname.sysname}")

        val ksuKernel = Natives.version
        pw.println("KernelSU: $ksuKernel")
        val safeMode = Natives.isSafeMode
        pw.println("SafeMode: $safeMode")
        val lkmMode = Natives.isLkmMode
        pw.println("LKM: $lkmMode")
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

