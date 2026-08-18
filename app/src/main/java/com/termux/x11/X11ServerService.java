package com.termux.x11;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.annotation.Nullable;

import app.trierarch.x11.X11Runtime;

/** Runs Xorg in its own process: Lorie exits that process when its server stops. */
public final class X11ServerService extends Service {
    private static final String TAG = "TrierarchX11";
    private final CmdEntryPoint command = new CmdEntryPoint();
    private volatile boolean ready;
    private boolean started;

    private final IX11Server.Stub binder = new IX11Server.Stub() {
        @Override public ParcelFileDescriptor getXConnection() {
            return ready ? command.getXConnection() : null;
        }
    };

    @Override public void onCreate() {
        super.onCreate();
        startServerOnce();
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        startServerOnce();
        return START_NOT_STICKY;
    }

    @Nullable @Override public IBinder onBind(Intent intent) {
        return binder;
    }

    private synchronized void startServerOnce() {
        if (started) return;
        started = true;
        new Thread(() -> {
            try {
                X11Runtime.prepareServerEnvironment(getApplicationContext());
                Looper.prepare();
                ready = CmdEntryPoint.start(new String[] { ":0", "-ac" });
                if (!ready) {
                    Log.e(TAG, "Lorie refused to start");
                    stopSelf();
                    return;
                }
                Looper.loop();
            } catch (Throwable error) {
                Log.e(TAG, "Unable to start Lorie", error);
                ready = false;
                stopSelf();
            }
        }, "Trierarch-X11-server").start();
    }
}
