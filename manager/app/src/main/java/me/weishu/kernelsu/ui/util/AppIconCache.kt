package me.weishu.kernelsu.ui.util

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.os.UserHandle
import android.util.LruCache
import androidx.core.graphics.createBitmap
import androidx.core.graphics.drawable.toDrawable
import androidx.core.graphics.scale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext

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
                val pm = context.packageManager
                var presampledBitmap: Bitmap? = null

                try {
                    val appRes = pm.getResourcesForApplication(applicationInfo)
                    val iconId = applicationInfo.icon

                    if (iconId != 0) {
                        val options = BitmapFactory.Options().apply {
                            inJustDecodeBounds = true
                        }
                        BitmapFactory.decodeResource(appRes, iconId, options)

                        if (options.outWidth > size * 6 || options.outHeight > size * 6) {
                            options.inSampleSize = calculateInSampleSize(options, size, size)
                            options.inJustDecodeBounds = false
                            presampledBitmap = BitmapFactory.decodeResource(appRes, iconId, options)
                        }
                    }
                } catch (_: Exception) {
                    // Ignore
                }

                val drawable = presampledBitmap?.toDrawable(context.resources)
                    ?: applicationInfo.loadUnbadgedIcon(pm)

                // Add system badges
                val handle = UserHandle.getUserHandleForUid(applicationInfo.uid)
                val badgedDrawable = try {
                    pm.getUserBadgedIcon(drawable, handle)
                } catch (_: Exception) {
                    drawable
                }

                // Convert to Bitmap for caching
                val bitmap = if (badgedDrawable is BitmapDrawable) {
                    badgedDrawable.bitmap
                } else {
                    val w = if (badgedDrawable.intrinsicWidth > 0) badgedDrawable.intrinsicWidth else size
                    val h = if (badgedDrawable.intrinsicHeight > 0) badgedDrawable.intrinsicHeight else size
                    createBitmap(w, h).also { bmp ->
                        val canvas = Canvas(bmp)
                        badgedDrawable.setBounds(0, 0, w, h)
                        badgedDrawable.draw(canvas)
                    }
                }

                // Resize to exact target size
                val resultBitmap = if (bitmap.width > size || bitmap.height > size) {
                    bitmap.scale(size, size)
                } else {
                    bitmap
                }

                // Convert to bitmap
                val gpuBitmap = try {
                    resultBitmap.copy(Bitmap.Config.HARDWARE, false)?.also {
                        if (resultBitmap !== bitmap) resultBitmap.recycle()
                    } ?: resultBitmap.also { it.prepareToDraw() }
                } catch (_: Exception) {
                    resultBitmap.also { it.prepareToDraw() }
                }

                synchronized(lruCache) {
                    lruCache.put(key, gpuBitmap)
                }

                gpuBitmap
            }
        }
    }

    private fun calculateInSampleSize(options: BitmapFactory.Options, reqWidth: Int, reqHeight: Int): Int {
        val height: Int = options.outHeight
        val width: Int = options.outWidth
        var inSampleSize = 1

        if (height > reqHeight || width > reqWidth) {
            while ((height / (inSampleSize * 2)) >= reqHeight && (width / (inSampleSize * 2)) >= reqWidth) {
                inSampleSize *= 2
            }
        }
        return inSampleSize
    }
}