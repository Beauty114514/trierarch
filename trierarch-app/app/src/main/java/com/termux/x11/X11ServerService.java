package com.termux.x11;

import android.app.Application;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import app.trierarch.R;
import app.trierarch.X11Runtime;

/**
 * Upstream-style separation: [CmdEntryPoint] runs in {@code :x11}, like {@code app_process} with a
 * dedicated Looper, not in the same process as the shell / Lorie [android.view.Surface] activity.
 * This avoids blocking the app UI / native re-entrancy (see project ANR traces in X11Runtime).
 */
public final class X11ServerService extends Service {
    private static final String TAG = "X11Server";
    private static final String NOTIF_CH = "trierarch_x11_cmd";
    private static final int NOTIF_ID = 0x5e11_0001;
    private static final Object lock = new Object();
    private static boolean cmdThreadRunning;

    @Override
    public void onCreate() {
        super.onCreate();
        startAsForeground();
        startCmdEntryInBackgroundIfNeeded();
    }

    private void startAsForeground() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel ch = new NotificationChannel(
                NOTIF_CH, "X11 display", NotificationManager.IMPORTANCE_LOW
            );
            ch.setDescription("In-process Xorg for :0 (Lorie client is in the main app)");
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) {
                nm.createNotificationChannel(ch);
            }
        }
        Notification n = new NotificationCompat.Builder(this, NOTIF_CH)
            .setContentTitle(getString(R.string.app_name) + " — X11 :0")
            .setContentText("Server (Termux-style isolated process)")
            .setSmallIcon(R.drawable.ic_x11_icon)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCategory(Notification.CATEGORY_SERVICE)
            .build();
        try {
            if (Build.VERSION.SDK_INT >= 34) {
                startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
            } else {
                startForeground(NOTIF_ID, n);
            }
        } catch (Exception e) {
            Log.w(TAG, "startForeground; retry without FGS type", e);
            startForeground(NOTIF_ID, n);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startAsForeground();
        startCmdEntryInBackgroundIfNeeded();
        return START_STICKY;
    }

    private void startCmdEntryInBackgroundIfNeeded() {
        final Application a = getApplication();
        synchronized (lock) {
            if (cmdThreadRunning) return;
            cmdThreadRunning = true;
        }
        new Thread(() -> {
            try {
                // TMPDIR, XKB unpack, etc. — may take seconds; not on the main / UI process.
                X11Runtime.applyLorieProcessEnvForX11ServerProcess(a);
                Looper.prepare();
                android.util.Log.i(TAG, "CmdEntryPoint.main(:0) starting in :x11");
                CmdEntryPoint.main(new String[] {":0", "-ac"});
            } catch (Throwable t) {
                Log.e(TAG, "CmdEntryPoint.main", t);
                synchronized (lock) {
                    cmdThreadRunning = false;
                }
            }
        }, "Trierarch-CmdEntryPoint-:x11").start();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
