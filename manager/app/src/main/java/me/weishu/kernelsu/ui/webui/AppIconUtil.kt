package me.weishu.kernelsu.ui.webui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.util.LruCache
import androidx.core.graphics.createBitmap
import androidx.core.graphics.scale
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel.Companion.getAppIconDrawable

object AppIconUtil {
    // Limit cache size to 200 icons
    private const val CACHE_SIZE = 200
    private val iconCache = LruCache<String?, Bitmap?>(CACHE_SIZE)

    @Synchronized
    fun loadAppIconSync(context: Context, packageName: String, sizePx: Int): Bitmap? {
        val cached = iconCache.get(packageName)
        if (cached != null) return cached

        try {
            val drawable = getAppIconDrawable(context, packageName) ?: return null
            val raw = drawableToBitmap(drawable, sizePx)
            val icon = raw.scale(sizePx, sizePx)
            iconCache.put(packageName, icon)
            return icon
        } catch (_: Exception) {
            return null
        }
    }

    private fun drawableToBitmap(drawable: Drawable, size: Int): Bitmap {
        if (drawable is BitmapDrawable) return drawable.bitmap

        val width = if (drawable.intrinsicWidth > 0) drawable.intrinsicWidth else size
        val height = if (drawable.intrinsicHeight > 0) drawable.intrinsicHeight else size

        val bmp = createBitmap(width, height)
        val canvas = Canvas(bmp)
        drawable.setBounds(0, 0, canvas.width, canvas.height)
        drawable.draw(canvas)
        return bmp
    }
}
