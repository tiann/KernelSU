package me.weishu.kernelsu.ui.util

import android.content.Context
import me.weishu.kernelsu.ui.screen.home.getManagerVersion
import java.io.File
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

fun getBugreportFile(context: Context): File {
    val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd_HH_mm")
    val current = LocalDateTime.now().format(formatter)
    val targetFile = File(context.cacheDir, "KernelSU_bugreport_${current}.tar.gz")

    val managerVersion = getManagerVersion(context)
    val shell = getRootShell(true)
    shell.newJob()
        .add("${getKsuDaemonPath()} bugreport collect -o ${targetFile.absolutePath} --manager-version '$managerVersion'")
        .exec()
    shell.newJob().add("chmod 0644 ${targetFile.absolutePath}").exec()

    return targetFile
}
