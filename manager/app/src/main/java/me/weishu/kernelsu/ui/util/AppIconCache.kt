package me.weishu.kernelsu.ui.util

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.util.LruCache
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import me.zhanghai.android.appiconloader.AppIconLoader

object AppIconCache {
    private val maxMemory = Runtime.getRuntime().maxMemory() / 1024
    private val cacheSize = (maxMemory / 8).toInt()
    private val loadSemaphore = Semaphore(4)

    private val lruCache = object : LruCache<String, Bitmap>(cacheSize) {
        override fun sizeOf(key: String, value: Bitmap): Int {
            return value.allocationByteCount / 1024
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
                val loader = AppIconLoader(size, true, context)
                val bitmap = loader.loadIcon(applicationInfo)

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
