package me.weishu.kernelsu.ui.util

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.os.Process
import android.os.UserManager
import android.util.LruCache
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ui.util.AppIconCache.getMainUserId
import me.zhanghai.android.appiconloader.AppIconLoader
import org.lsposed.hiddenapibypass.HiddenApiBypass

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
    private val maxMemory = Runtime.getRuntime().maxMemory() / 1024
    private val cacheSize = (maxMemory / 8).toInt()
    private val loadSemaphore = Semaphore(4)

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

    private var cachedMainUserId: Int? = null

    internal fun getMainUserId(context: Context): Int {
        cachedMainUserId?.let { return it }

        synchronized(this) {
            cachedMainUserId?.let { return it }

            val um = context.getSystemService(Context.USER_SERVICE) as UserManager
            val profiles = um.userProfiles
            var foundMainUserId = 0
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
            } catch (_: Exception) {
                foundMainUserId = userId(Process.myUid())
            }
            cachedMainUserId = foundMainUserId
            return foundMainUserId
        }
    }

    fun getFromCache(applicationInfo: ApplicationInfo): Bitmap? {
        val key = buildKey(applicationInfo)
        synchronized(lruCache) { return lruCache.get(key) }
    }

    private fun buildKey(applicationInfo: ApplicationInfo): String {
        return "${applicationInfo.packageName}:${applicationInfo.uid}:${applicationInfo.sourceDir}"
    }

    suspend fun loadIcon(context: Context, applicationInfo: ApplicationInfo, size: Int): Bitmap {
        val key = buildKey(applicationInfo)

        synchronized(lruCache) {
            val cached = lruCache.get(key)
            if (cached != null) return cached
        }

        return loadSemaphore.withPermit {
            synchronized(lruCache) {
                val cached = lruCache.get(key)
                if (cached != null) return@withPermit cached
            }

            withContext(Dispatchers.IO) {
                val loader = AppIconLoader(size, false, context)

                val bitmap = if (isOtherProfileGroupUser(applicationInfo.uid)) {
                    loader.loadIcon(applicationInfo.withMainUserUid(context))
                } else {
                    try {
                        loader.loadIcon(applicationInfo)
                    } catch (_: Exception) {
                        markOtherProfileGroupUser(applicationInfo.uid)
                        loader.loadIcon(applicationInfo.withMainUserUid(context))
                    }
                }

                val gpuBitmap = try {
                    bitmap.copy(Bitmap.Config.HARDWARE, false)?.also {
                        bitmap.recycle()
                    } ?: bitmap.also { it.prepareToDraw() }
                } catch (_: Exception) {
                    bitmap.also { it.prepareToDraw() }
                }

                synchronized(lruCache) {
                    lruCache.put(key, gpuBitmap)
                }

                gpuBitmap
            }
        }
    }
}
