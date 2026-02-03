package me.weishu.kernelsu.ui.util.module

import android.app.AppOpsManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log
import android.widget.Toast
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.graphics.scale
import androidx.core.net.toUri
import com.topjohnwu.superuser.io.SuFile
import com.topjohnwu.superuser.io.SuFileInputStream
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.MainActivity
import me.weishu.kernelsu.ui.util.getRootShell
import me.weishu.kernelsu.ui.webui.WebUIActivity
import java.util.Locale

object Shortcut {

    private const val TAG = "ModuleShortcut"

    fun createModuleActionShortcut(
        context: Context,
        moduleId: String,
        name: String,
        iconUri: String?
    ) {
        val shortcutId = "module_action_$moduleId"
        val shortcutIntent = Intent(context, MainActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            putExtra("shortcut_type", "module_action")
            putExtra("module_id", moduleId)
            addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        }
        createModuleShortcut(
            context = context,
            moduleId = moduleId,
            name = name,
            iconUri = iconUri,
            shortcutId = shortcutId,
            shortcutIntent = shortcutIntent,
            logPrefix = "createModuleActionShortcut"
        )
    }

    fun createModuleWebUiShortcut(
        context: Context,
        moduleId: String,
        name: String,
        iconUri: String?
    ) {
        val shortcutId = "module_webui_$moduleId"
        val shortcutIntent = Intent(context, WebUIActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = "kernelsu://webui/$moduleId".toUri()
            putExtra("id", moduleId)
            putExtra("name", name)
            putExtra("from_webui_shortcut", true)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        }
        createModuleShortcut(
            context = context,
            moduleId = moduleId,
            name = name,
            iconUri = iconUri,
            shortcutId = shortcutId,
            shortcutIntent = shortcutIntent,
            logPrefix = "createModuleWebUiShortcut"
        )
    }

    private fun createModuleShortcut(
        context: Context,
        moduleId: String,
        name: String,
        iconUri: String?,
        shortcutId: String,
        shortcutIntent: Intent,
        logPrefix: String
    ) {
        val hasPinned = hasPinnedShortcut(context, shortcutId)
        Log.d(TAG, "$logPrefix: shortcutId=$shortcutId, hasPinned=$hasPinned")

        val iconCompat = createShortcutIcon(context, iconUri)
        val finalIcon = iconCompat ?: IconCompat.createWithResource(context, R.mipmap.ic_launcher)

        val shortcut = ShortcutInfoCompat.Builder(context, shortcutId)
            .setShortLabel(name)
            .setIntent(shortcutIntent)
            .setIcon(finalIcon)
            .build()

        try {
            Log.d(TAG, "$logPrefix: pushDynamicShortcut() called for moduleId=$moduleId")
            ShortcutManagerCompat.pushDynamicShortcut(context, shortcut)
        } catch (t: Throwable) {
            Log.w(TAG, "$logPrefix: pushDynamicShortcut() threw exception for moduleId=$moduleId: ${t.message}", t)
        }

        if (hasPinned) {
            Log.d(TAG, "$logPrefix: detected existing pinned shortcut, updating only")
            Toast.makeText(context, context.getString(R.string.module_shortcut_updated), Toast.LENGTH_SHORT).show()
            return
        }

        val manufacturer = Build.MANUFACTURER.lowercase(Locale.ROOT)
        val initialState = getShortcutPermissionState(context)
        Log.d(TAG, "$logPrefix: initial permission state=$initialState")
        if (manufacturer.contains("xiaomi") && initialState != ShortcutPermissionState.Granted) {
            Log.d(TAG, "$logPrefix: device is Xiaomi, trying to grant via root shell")
            val rootSuccess = tryGrantMiuiShortcutPermissionByRoot(context)
            Log.d(TAG, "$logPrefix: root grant attempt success=$rootSuccess")
            val afterState = getShortcutPermissionState(context)
            Log.d(TAG, "$logPrefix: state after root attempt=$afterState")
            if (afterState != ShortcutPermissionState.Granted) {
                Log.d(TAG, "$logPrefix: still not Granted after root, showing hint")
                showShortcutPermissionHint(context)
                return
            }
        } else if (initialState == ShortcutPermissionState.Denied || initialState == ShortcutPermissionState.Ask) {
            Log.d(TAG, "$logPrefix: permission not granted (state=$initialState), showing hint first")
            showShortcutPermissionHint(context)
            return
        }
        if (!ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            Log.w(TAG, "$logPrefix: requestPinShortcut not supported on this launcher")
            Toast.makeText(
                context,
                context.getString(R.string.module_shortcut_not_supported),
                Toast.LENGTH_LONG
            ).show()
            return
        }

        val pinned = try {
            Log.d(TAG, "$logPrefix: requestPinShortcut() called for moduleId=$moduleId")
            val result = ShortcutManagerCompat.requestPinShortcut(context, shortcut, null)
            Log.d(TAG, "$logPrefix: requestPinShortcut() result=$result")
            result
        } catch (t: Throwable) {
            Log.w(TAG, "$logPrefix: requestPinShortcut() threw exception for moduleId=$moduleId: ${t.message}", t)
            false
        }

        if (pinned) {
            Log.d(TAG, "$logPrefix: pinned shortcut created successfully for moduleId=$moduleId")
            Toast.makeText(
                context,
                context.getString(R.string.module_shortcut_created),
                Toast.LENGTH_SHORT
            ).show()
        } else {
            Log.w(TAG, "$logPrefix: pinned shortcut not created, showing permission hint for moduleId=$moduleId")
            showShortcutPermissionHint(context)
        }
    }

    fun hasModuleActionShortcut(context: Context, moduleId: String): Boolean {
        val id = "module_action_$moduleId"
        return hasPinnedShortcut(context, id)
    }

    fun hasModuleWebUiShortcut(context: Context, moduleId: String): Boolean {
        val id = "module_webui_$moduleId"
        return hasPinnedShortcut(context, id)
    }

    fun deleteModuleActionShortcut(context: Context, moduleId: String) {
        deleteShortcut(context, "module_action_$moduleId")
    }

    fun deleteModuleWebUiShortcut(context: Context, moduleId: String) {
        deleteShortcut(context, "module_webui_$moduleId")
    }

    fun loadShortcutBitmap(context: Context, iconUri: String?): Bitmap? {
        if (iconUri.isNullOrBlank()) {
            return null
        }
        return try {
            val uri = iconUri.toUri()
            Log.d(TAG, "loadShortcutBitmap: loading bitmap from uri=$uri")
            val rawBitmap = if (uri.scheme.equals("su", ignoreCase = true)) {
                val path = uri.path ?: ""
                if (path.isNotBlank()) {
                    val shell = getRootShell(true)
                    val suFile = SuFile(path)
                    suFile.shell = shell
                    SuFileInputStream.open(suFile).use { input ->
                        BitmapFactory.decodeStream(input)
                    }
                } else null
            } else {
                context.contentResolver.openInputStream(uri)?.use { input ->
                    BitmapFactory.decodeStream(input)
                }
            }
            if (rawBitmap != null) {
                Log.d(TAG, "loadShortcutBitmap: decoded bitmap successfully")
                val w = rawBitmap.width
                val h = rawBitmap.height
                val side = minOf(w, h)
                val x = (w - side) / 2
                val y = (h - side) / 2
                val square = try {
                    Bitmap.createBitmap(rawBitmap, x, y, side, side)
                } catch (_: Throwable) {
                    rawBitmap
                }
                if (square !== rawBitmap && !rawBitmap.isRecycled) {
                    rawBitmap.recycle()
                }
                if (side > 512) {
                    try {
                        val scaled = square.scale(512, 512)
                        if (scaled !== square && !square.isRecycled) {
                            square.recycle()
                        }
                        scaled
                    } catch (_: Throwable) {
                        square
                    }
                } else {
                    square
                }
            } else {
                Log.w(TAG, "loadShortcutBitmap: bitmap decode returned null")
                null
            }
        } catch (t: Throwable) {
            Log.w(TAG, "loadShortcutBitmap: exception when loading icon from uri=$iconUri: ${t.message}", t)
            null
        }
    }

    private fun createShortcutIcon(context: Context, iconUri: String?): IconCompat? {
        val bitmap = loadShortcutBitmap(context, iconUri) ?: return null
        return IconCompat.createWithBitmap(bitmap)
    }

    private fun hasPinnedShortcut(context: Context, id: String): Boolean {
        return try {
            val shortcuts = ShortcutManagerCompat.getShortcuts(
                context,
                ShortcutManagerCompat.FLAG_MATCH_PINNED
            )
            val exists = shortcuts.any { it.id == id && it.isEnabled }
            Log.d(TAG, "hasPinnedShortcut: id=$id, exists=$exists")
            exists
        } catch (t: Throwable) {
            Log.w(TAG, "hasPinnedShortcut: exception for id=$id: ${t.message}", t)
            false
        }
    }

    private fun deleteShortcut(context: Context, id: String) {
        try {
            ShortcutManagerCompat.removeDynamicShortcuts(context, listOf(id))
            Log.d(TAG, "deleteShortcut: removed dynamic shortcut id=$id")
        } catch (t: Throwable) {
            Log.w(TAG, "deleteShortcut: removeDynamicShortcuts exception for id=$id: ${t.message}", t)
        }
        try {
            ShortcutManagerCompat.disableShortcuts(context, listOf(id), "")
            Log.d(TAG, "deleteShortcut: disabled shortcut id=$id")
        } catch (t: Throwable) {
            Log.w(TAG, "deleteShortcut: disableShortcuts exception for id=$id: ${t.message}", t)
        }
    }

    private enum class ShortcutPermissionState {
        Granted,
        Denied,
        Ask,
        Unknown
    }

    private fun checkMiuiShortcutPermission(context: Context): ShortcutPermissionState {
        return try {
            val appOps = context.getSystemService(Context.APP_OPS_SERVICE) as? AppOpsManager
                ?: return ShortcutPermissionState.Unknown
            val pkg = context.applicationContext.packageName
            val uid = context.applicationInfo.uid
            Log.d(TAG, "checkMiuiShortcutPermission: pkg=$pkg, uid=$uid")
            val appOpsClass = Class.forName(AppOpsManager::class.java.name)
            val method = appOpsClass.getDeclaredMethod(
                "checkOpNoThrow",
                Integer.TYPE,
                Integer.TYPE,
                String::class.java
            )
            val result = method.invoke(appOps, 10017, uid, pkg)?.toString()
            if (result == null) {
                Log.w(TAG, "checkMiuiShortcutPermission: checkOpNoThrow returned null")
                return ShortcutPermissionState.Unknown
            }
            Log.d(TAG, "checkMiuiShortcutPermission: raw result=$result")
            val state = when (result) {
                "0" -> ShortcutPermissionState.Granted
                "1" -> ShortcutPermissionState.Denied
                "5" -> ShortcutPermissionState.Ask
                else -> ShortcutPermissionState.Unknown
            }
            Log.d(TAG, "checkMiuiShortcutPermission: mapped state=$state")
            state
        } catch (t: Throwable) {
            Log.w(TAG, "checkMiuiShortcutPermission: exception=${t.message}", t)
            ShortcutPermissionState.Unknown
        }
    }

    private fun checkOppoShortcutPermission(context: Context): ShortcutPermissionState {
        val resolver = context.contentResolver ?: run {
            Log.w(TAG, "checkOppoShortcutPermission: contentResolver is null")
            return ShortcutPermissionState.Unknown
        }
        val uri = "content://settings/secure/launcher_shortcut_permission_settings".toUri()
        val cursor = resolver.query(uri, null, null, null, null) ?: run {
            Log.w(TAG, "checkOppoShortcutPermission: query returned null cursor, uri=$uri")
            return ShortcutPermissionState.Unknown
        }
        cursor.use { c ->
            val pkg = context.applicationContext.packageName
            val index = c.getColumnIndex("value")
            if (index == -1) {
                Log.w(TAG, "checkOppoShortcutPermission: 'value' column not found")
                return ShortcutPermissionState.Unknown
            }
            Log.d(TAG, "checkOppoShortcutPermission: pkg=$pkg")
            while (c.moveToNext()) {
                val value = c.getString(index)
                if (!value.isNullOrEmpty()) {
                    Log.d(TAG, "checkOppoShortcutPermission: row value=$value")
                    if (value.contains("$pkg, 1")) {
                        Log.d(TAG, "checkOppoShortcutPermission: detected Granted")
                        return ShortcutPermissionState.Granted
                    }
                    if (value.contains("$pkg, 0")) {
                        Log.d(TAG, "checkOppoShortcutPermission: detected Denied")
                        return ShortcutPermissionState.Denied
                    }
                }
            }
        }
        return ShortcutPermissionState.Unknown
    }

    private fun tryGrantMiuiShortcutPermissionByRoot(context: Context): Boolean {
        val pkg = context.applicationContext.packageName
        val cmd = "appops set $pkg 10017 allow"
        return try {
            val shell = getRootShell()
            val result = shell.newJob().add(cmd).exec()
            Log.d(TAG, "tryGrantMiuiShortcutPermissionByRoot: cmd=$cmd, code=${result.code}, isSuccess=${result.isSuccess}")
            result.isSuccess
        } catch (t: Throwable) {
            Log.w(TAG, "tryGrantMiuiShortcutPermissionByRoot: exception=${t.message}", t)
            false
        }
    }

    private fun getShortcutPermissionState(context: Context): ShortcutPermissionState {
        val manufacturer = Build.MANUFACTURER.lowercase(Locale.ROOT)
        return when {
            manufacturer.contains("xiaomi") -> checkMiuiShortcutPermission(context)
            manufacturer.contains("oppo") -> checkOppoShortcutPermission(context)
            else -> ShortcutPermissionState.Unknown
        }
    }

    private fun showShortcutPermissionHint(context: Context) {
        val manufacturer = Build.MANUFACTURER.lowercase(Locale.ROOT)
        Log.d(TAG, "showShortcutPermissionHint: manufacturer=$manufacturer")
        val state = getShortcutPermissionState(context)
        val messageRes = when {
            manufacturer.contains("xiaomi") -> R.string.module_shortcut_permission_tip_xiaomi
            manufacturer.contains("oppo") -> R.string.module_shortcut_permission_tip_oppo
            else -> R.string.module_shortcut_permission_tip_default
        }
        Log.d(TAG, "showShortcutPermissionHint: state=$state, messageRes=$messageRes")
        Toast.makeText(context, context.getString(messageRes), Toast.LENGTH_LONG).show()
        if (state != ShortcutPermissionState.Granted) {
            Log.d(TAG, "showShortcutPermissionHint: state is not Granted, opening app details settings")
            openAppDetailsSettings(context)
        }
    }

    private fun openAppDetailsSettings(context: Context) {
        val intent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
            data = Uri.fromParts("package", context.packageName, null)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        try {
            Log.d(TAG, "openAppDetailsSettings: launching settings for package=${context.packageName}")
            context.startActivity(intent)
        } catch (t: Throwable) {
            Log.w(TAG, "openAppDetailsSettings: failed to launch settings: ${t.message}", t)
        }
    }
}
