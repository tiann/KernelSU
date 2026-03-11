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
            Log.d(TAG, "executing magica ...");
            Os.setuid(0);
            Log.d(TAG, "set uid 0 ...");
            var nullFile = new File("/dev/null");
            var proc = new ProcessBuilder()
                    .command(f.getAbsolutePath(), "late-load", "--magica", "5555")
                    .redirectOutput(nullFile)
                    .redirectInput(nullFile)
                    .redirectError(nullFile)
                    .start()
            ;
            var res = proc.waitFor();
            Log.d(TAG, "res=" + res);
            // we need to exit to prevent from app being blocked
            System.exit(0);
            // Os.setuid(uid);
        } catch (Throwable t) {
            Log.e(TAG, "failed to late load", t);
        }
    }
}
