package me.weishu.kernelsu.magica;

import android.annotation.SuppressLint;
import android.app.ZygotePreload;
import android.content.pm.ApplicationInfo;
import android.system.Os;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.File;

import me.weishu.kernelsu.Natives;

@SuppressLint("NewApi")
public class AppZygotePreload implements ZygotePreload {
    public static final String TAG = "KernelSUMagica";

    @Override
    public void doPreload(@NonNull ApplicationInfo appInfo) {
        try {
            var uid = Os.getuid();
            Log.d(TAG, "set uid 0 ...");
            Os.setuid(0);
            Log.d(TAG, "executing magica ...");
            Natives.forkDontCareAndExecKsud(appInfo.nativeLibraryDir + File.separator + "libksud.so");
            Os.setuid(uid);
        } catch (Throwable t) {
            Log.e(TAG, "failed to late load", t);
        }
    }
}
