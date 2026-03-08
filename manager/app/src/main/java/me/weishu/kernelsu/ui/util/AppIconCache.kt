package me.weishu.kernelsu.ui.util

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.UserHandle
import android.util.LruCache
import androidx.core.graphics.createBitmap
import androidx.core.graphics.drawable.toDrawable
import androidx.core.graphics.scale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object AppIconCache {
    private val maxMemory = Runtime.getRuntime().maxMemory() / 1024
    private val cacheSize = (maxMemory / 8).toInt()

    private val lruCache = object : LruCache<String, Bitmap>(cacheSize) {
        override fun sizeOf(key: String, value: Bitmap): Int {
            return value.byteCount / 1024
        }
    }

    suspend fun loadIcon(context: Context, applicationInfo: ApplicationInfo, size: Int): Bitmap {
        val key = "${applicationInfo.packageName}:${applicationInfo.uid}"

        synchronized(lruCache) {
            val cachedBitmap = lruCache.get(key)
            if (cachedBitmap != null) return cachedBitmap
        }

        return withContext(Dispatchers.IO) {
            val pm = context.packageManager
            var finalDrawable: Drawable? = null

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

                        val scaledBitmap = BitmapFactory.decodeResource(appRes, iconId, options)
                        if (scaledBitmap != null) {
                            finalDrawable = scaledBitmap.toDrawable(context.resources)
                        }
                    }
                }
            } catch (_: Exception) {
                // Ignore
            }

            if (finalDrawable == null) {
                finalDrawable = applicationInfo.loadUnbadgedIcon(pm)
            }

            // Add system badges
            val handle = UserHandle.getUserHandleForUid(applicationInfo.uid)
            val badgedDrawable = try {
                pm.getUserBadgedIcon(finalDrawable, handle)
            } catch (_: Exception) {
                finalDrawable
            }

            // Convert to Bitmap for caching
            val bitmap = if (badgedDrawable is BitmapDrawable) {
                badgedDrawable.bitmap
            } else {
                val w = if (badgedDrawable.intrinsicWidth > 0) badgedDrawable.intrinsicWidth else size
                val h = if (badgedDrawable.intrinsicHeight > 0) badgedDrawable.intrinsicHeight else size
                val bmp = createBitmap(w, h)
                val canvas = Canvas(bmp)
                badgedDrawable.setBounds(0, 0, canvas.width, canvas.height)
                badgedDrawable.draw(canvas)
                bmp
            }

            // Resize if too large (consistent with original logic)
            val resultBitmap = if (bitmap.width > size * 2 || bitmap.height > size * 2) {
                val scaled = bitmap.scale(size, size)
                scaled
            } else {
                bitmap
            }

            synchronized(lruCache) {
                lruCache.put(key, resultBitmap)
            }

            resultBitmap
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
