package me.weishu.kernelsu.ui.webui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import java.util.HashMap;
import java.util.Map;
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel;

public class AppIconUtil {
    private static final Map<String, Bitmap> iconCache = new HashMap<>();

    public static Bitmap loadAppIconSync(Context context, String packageName, int sizePx) {
        Bitmap cached = iconCache.get(packageName);
        if (cached != null) return cached;

        try {
            Drawable drawable = SuperUserViewModel.getAppIconDrawable(context, packageName);
            Bitmap raw = drawableToBitmap(drawable, sizePx);
            Bitmap icon = Bitmap.createScaledBitmap(raw, sizePx, sizePx, true);
            iconCache.put(packageName, icon);
            return icon;
        } catch (Exception e) {
            return null;
        }
    }

    private static Bitmap drawableToBitmap(Drawable drawable, int size) {
        if (drawable instanceof BitmapDrawable) return ((BitmapDrawable) drawable).getBitmap();

        int width = drawable.getIntrinsicWidth() > 0 ? drawable.getIntrinsicWidth() : size;
        int height = drawable.getIntrinsicHeight() > 0 ? drawable.getIntrinsicHeight() : size;

        Bitmap bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bmp);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bmp;
    }
}
