package me.weishu.kernelsu.ui.webui.util

import android.app.Activity
import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Build
import me.weishu.kernelsu.ui.webui.WebUIActivity

fun Activity.setTaskDescription(label: String, icon: Bitmap? = null) {
    if (icon != null) {
        @Suppress("DEPRECATION")
        setTaskDescription(ActivityManager.TaskDescription(label, icon))
    } else {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            @Suppress("DEPRECATION")
            setTaskDescription(ActivityManager.TaskDescription(label))
        } else {
            val taskDescription = ActivityManager.TaskDescription.Builder()
                .setLabel(label)
                .build()
            setTaskDescription(taskDescription)
        }
    }
}

fun createWebuiIntent(context: Context, moduleId: String): Intent {
    val canonicalUri = Uri.Builder()
        .scheme("ksu")
        .authority("webui")
        .appendQueryParameter("id", moduleId)
        .build()
    return Intent(Intent.ACTION_VIEW, canonicalUri, context, WebUIActivity::class.java).apply {
        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
    }
}
