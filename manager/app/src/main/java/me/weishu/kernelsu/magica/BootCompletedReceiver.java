package me.weishu.kernelsu.magica;

import static me.weishu.kernelsu.magica.AppZygotePreload.TAG;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import me.weishu.kernelsu.ui.util.KsuCliKt;

public class BootCompletedReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }
        var action = intent.getAction();
        if (!Intent.ACTION_LOCKED_BOOT_COMPLETED.equals(action)
                && !Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            return;
        }
        if (KsuCliKt.rootAvailable()) return;

        PendingResult pendingResult = goAsync();
        try {
            MagicaService.start(context, pendingResult::finish);
            Log.i(TAG, "MagicaService bootstrap started from boot action: " + action);
        } catch (Throwable e) {
            pendingResult.finish();
            Log.e(TAG, "Failed to bootstrap MagicaService from boot action: " + action, e);
        }
    }
}
