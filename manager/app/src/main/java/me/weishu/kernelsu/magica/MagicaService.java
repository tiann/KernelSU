package me.weishu.kernelsu.magica;

import static me.weishu.kernelsu.magica.AppZygotePreload.TAG;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.Nullable;

import java.util.concurrent.atomic.AtomicBoolean;

public class MagicaService extends Service {
    private static final long BOOTSTRAP_TIMEOUT_MS = 8_000L;
    private static final Handler MAIN_HANDLER = new Handler(Looper.getMainLooper());

    public static void setEnabled(Context context, boolean enabled) {
        context.getPackageManager().setComponentEnabledSetting(
                new ComponentName(context, MagicaService.class),
                enabled
                        ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                        : PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP);
    }

    public static void start(Context context) {
        start(context, null);
    }

    public static void start(Context context, @Nullable Runnable onFinished) {
        Context appContext = context.getApplicationContext();
        BootstrapConnection connection = new BootstrapConnection(appContext, onFinished);

        setEnabled(appContext, true);
        connection.scheduleTimeout();
        try {
            if (!appContext.bindService(
                    new Intent(appContext, MagicaService.class),
                    connection,
                    Context.BIND_AUTO_CREATE)) {
                connection.abort();
                throw new IllegalStateException("Failed to bind MagicaService");
            }
        } catch (RuntimeException e) {
            connection.abort();
            throw e;
        }
    }

    private static final class BootstrapConnection implements ServiceConnection {
        private final Context context;
        @Nullable private final Runnable onFinished;
        private final AtomicBoolean finished = new AtomicBoolean();
        private final Runnable timeout = () -> finish(true);

        BootstrapConnection(Context context, @Nullable Runnable onFinished) {
            this.context = context;
            this.onFinished = onFinished;
        }

        void scheduleTimeout() {
            MAIN_HANDLER.postDelayed(timeout, BOOTSTRAP_TIMEOUT_MS);
        }

        void abort() {
            if (!finished.compareAndSet(false, true)) return;
            MAIN_HANDLER.removeCallbacks(timeout);
            setEnabled(context, false);
        }

        private void finish(boolean unbind) {
            if (!finished.compareAndSet(false, true)) return;

            MAIN_HANDLER.removeCallbacks(timeout);
            try {
                setEnabled(context, false);
            } catch (RuntimeException e) {
                Log.e(TAG, "Failed to disable MagicaService", e);
            }

            if (unbind) {
                try {
                    context.unbindService(this);
                } catch (IllegalArgumentException ignored) {
                }
            }

            if (onFinished != null) {
                onFinished.run();
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            finish(true);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            finish(true);
        }

        @Override
        public void onBindingDied(ComponentName name) {
            finish(true);
        }

        @Override
        public void onNullBinding(ComponentName name) {
            finish(true);
        }
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return new Binder();
    }
}
