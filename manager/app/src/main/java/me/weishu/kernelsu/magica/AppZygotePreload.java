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

    private static native void forkDontCareAndExecKsud(String ksudPath);

    @Override
    public void doPreload(@NonNull ApplicationInfo appInfo) {
        File f = new File(appInfo.nativeLibraryDir, "libksud.so");
        try {
            System.loadLibrary("kernelsu");
            var uid = Os.getuid();
            Log.d(TAG, "set uid 0 ...");
            Os.setuid(0);
            Log.d(TAG, "executing magica ...");
            forkDontCareAndExecKsud(f.getAbsolutePath());
            Os.setuid(uid);
        } catch (Throwable t) {
            Log.e(TAG, "failed to late load", t);
        }
    }
}
