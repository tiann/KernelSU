package me.weishu.kernelsu.ui.util

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.os.Process
import android.os.UserManager
import android.util.Log
import android.util.LruCache
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ui.util.AppIconCache.getMainUserId
import me.zhanghai.android.appiconloader.AppIconLoader
import org.lsposed.hiddenapibypass.HiddenApiBypass
import java.util.concurrent.ConcurrentHashMap

/**
 * Remap the uid to the non-profile user possible to avoid SecurityException
 * when loading icons for cross-profileGroup apps.
 * See: https://github.com/zhanghai/AppIconLoader/issues/3
 */
fun ApplicationInfo.withMainUserUid(context: Context): ApplicationInfo {
    val mainUserId = getMainUserId(context)
    val appId = this.uid % 100000
    val targetUid = mainUserId * 100000 + appId

    if (this.uid == targetUid) return this
    return ApplicationInfo(this).apply { uid = targetUid }
}

object AppIconCache {
    private const val TAG = "AppIconCache"
    private val maxMemory = Runtime.getRuntime().maxMemory() / 1024
    private val cacheSize = (maxMemory / 8).toInt()
    private val loadSemaphore = Semaphore(4)
    private val iconLoaders = ConcurrentHashMap<Int, AppIconLoader>()

    private val lruCache = object : LruCache<String, Bitmap>(cacheSize) {
        override fun sizeOf(key: String, value: Bitmap): Int {
            return value.allocationByteCount / 1024
        }
    }

    @Volatile
    private var otherProfileGroupUserIds = setOf<Int>()

    private fun userId(uid: Int): Int = uid / 100000

    private fun isOtherProfileGroupUser(uid: Int): Boolean {
        return otherProfileGroupUserIds.contains(userId(uid))
    }

    private fun markOtherProfileGroupUser(uid: Int) {
        val id = userId(uid)
        if (!otherProfileGroupUserIds.contains(id)) {
            synchronized(this) {
                otherProfileGroupUserIds = otherProfileGroupUserIds + id
            }
        }
    }

    @Volatile
    private var cachedMainUserId: Int? = null

    internal fun getMainUserId(context: Context): Int {
        cachedMainUserId?.let { return it }

        synchronized(this) {
            cachedMainUserId?.let { return it }

            val um = context.getSystemService(Context.USER_SERVICE) as UserManager
            val profiles = um.userProfiles
            var foundMainUserId = userId(Process.myUid())
            try {
                HiddenApiBypass.addHiddenApiExemptions("Landroid/os/UserManager;->hasBadge(I)Z")
                val hasBadgeMethod = UserManager::class.java.getMethod("hasBadge", Int::class.javaPrimitiveType)
                for (profile in profiles) {
                    val id = profile.hashCode()
                    val hasBadge = hasBadgeMethod.invoke(um, id) as Boolean
                    if (!hasBadge) {
                        foundMainUserId = id
                        break
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to resolve main user id via hasBadge", e)
            }
            cachedMainUserId = foundMainUserId
            return foundMainUserId
        }
    }

    private fun buildKey(applicationInfo: ApplicationInfo, size: Int): String {
        return "${applicationInfo.packageName}:${applicationInfo.uid}:${applicationInfo.sourceDir}:${size}"
    }

    fun getCached(applicationInfo: ApplicationInfo, size: Int): Bitmap? {
        val key = buildKey(applicationInfo, size)
        return lruCache.get(key)?.takeIf { !it.isRecycled }
    }

    /**
     * Load icon bitmap using AppIconLoader with cross-profile fallback.
     * Called on IO thread, returns a software bitmap.
     */
    private fun loadIconBitmap(context: Context, applicationInfo: ApplicationInfo, size: Int): Bitmap {
        val loader = iconLoaders.getOrPut(size) {
            AppIconLoader(size, false, context.applicationContext)
        }

        return if (isOtherProfileGroupUser(applicationInfo.uid)) {
            loader.loadIcon(applicationInfo.withMainUserUid(context))
        } else {
            try {
                loader.loadIcon(applicationInfo)
            } catch (_: SecurityException) {
                Log.d(TAG, "SecurityException loading icon for userId ${userId(applicationInfo.uid)}, retrying with main user uid")
                markOtherProfileGroupUser(applicationInfo.uid)
                loader.loadIcon(applicationInfo.withMainUserUid(context))
            }
        }
    }

    /**
     * Synchronous icon loading for non-UI callers (e.g. WebView shouldInterceptRequest).
     * Returns a software bitmap suitable for compress/encode. Does NOT copy to HARDWARE config.
     */
    fun loadIconSync(context: Context, applicationInfo: ApplicationInfo, size: Int): Bitmap {
        val key = buildKey(applicationInfo, size)

        lruCache.get(key)?.let {
            if (!it.isRecycled) return it
            lruCache.remove(key)
        }

        val bitmap = loadIconBitmap(context, applicationInfo, size)
        bitmap.prepareToDraw()
        lruCache.put(key, bitmap)
        return bitmap
    }

    suspend fun loadIcon(context: Context, applicationInfo: ApplicationInfo, size: Int): Bitmap {
        val key = buildKey(applicationInfo, size)

        lruCache.get(key)?.let {
            if (!it.isRecycled) return it
            lruCache.remove(key)
        }

        return loadSemaphore.withPermit {
            lruCache.get(key)?.let {
                if (!it.isRecycled) return@withPermit it
                lruCache.remove(key)
            }

            withContext(Dispatchers.IO) {
                val bitmap = loadIconBitmap(context, applicationInfo, size)

                val gpuBitmap = try {
                    bitmap.copy(Bitmap.Config.HARDWARE, false)?.also {
                        bitmap.recycle()
                    } ?: bitmap.also { it.prepareToDraw() }
                } catch (e: Exception) {
                    Log.d(TAG, "Failed to copy bitmap to HARDWARE config", e)
                    bitmap.also { it.prepareToDraw() }
                }

                lruCache.put(key, gpuBitmap)

                gpuBitmap
            }
        }
    }
}
