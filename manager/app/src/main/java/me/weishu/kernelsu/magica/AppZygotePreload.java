package me.weishu.kernelsu.magica;

import android.annotation.SuppressLint;
import android.app.ZygotePreload;
import android.content.pm.ApplicationInfo;
import android.os.Process;
import android.system.Os;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.File;

@SuppressLint("NewApi")
public class AppZygotePreload implements ZygotePreload {
    public static final String TAG = "KernelSUMagica";

    @Override
    public void doPreload(@NonNull ApplicationInfo appInfo) {
        File f = new File(appInfo.nativeLibraryDir, "libksud.so");
        try {
            var uid = Os.getuid();
            Os.setuid(0);
            Runtime.getRuntime().exec(new String[] {f.getAbsolutePath(), "late-load", ""});
            Os.setuid(uid);
        } catch (Throwable t) {
            Log.e(TAG, "failed to late load", t);
        }
    }
}
