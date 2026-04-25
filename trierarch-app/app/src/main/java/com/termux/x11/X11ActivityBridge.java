package com.termux.x11;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;

import androidx.annotation.Nullable;

/**
 * Holds the embedding host (Trierarch activity + {@link LorieView}) for Termux:X11 code that
 * historically expected {@link MainActivity#getInstance()}.
 */
public final class X11ActivityBridge {
    private static final String TAG = "X11ActivityBridge";

    private static volatile Activity hostActivity;
    private static volatile LorieView lorieView;
    private static volatile Prefs prefs;

    private X11ActivityBridge() {}

    public static void setHostActivity(@Nullable Activity a) {
        hostActivity = a;
    }

    @Nullable
    public static Activity getHostActivity() {
        return hostActivity;
    }

    public static void setLorieView(@Nullable LorieView v) {
        lorieView = v;
    }

    @Nullable
    public static LorieView getLorieView() {
        return lorieView;
    }

    /** @noinspection SameParameterValue */
    public static synchronized Prefs getOrCreatePrefs(Context anyContext) {
        if (prefs == null && anyContext != null) {
            prefs = new Prefs(anyContext.getApplicationContext());
            Log.i(TAG, "Prefs initialized for X11 embedding");
        }
        return prefs;
    }

    /**
     * Applies output (resolution / scale) changes from {@link Prefs} and re-measures the embedded
     * {@link LorieView} if one is registered — same end state as changing options in the X11
     * preference screen.
     */
    public static void refreshLorieFromOutputPreferences(@Nullable Context anyContext) {
        Prefs p = getOrCreatePrefs(anyContext);
        if (p == null) return;
        LorieView lv = lorieView;
        if (lv == null) return;
        Handler h = (Looper.getMainLooper() != null) ? new Handler(Looper.getMainLooper()) : null;
        if (h != null) {
            h.post(() -> {
                lv.reloadPreferences(p);
                lv.requestLayout();
                lv.triggerCallback();
            });
        } else {
            lv.reloadPreferences(p);
            lv.requestLayout();
            lv.triggerCallback();
        }
    }

    /** Keyboard path used by {@link LorieView}; host is Compose — no Termux key routing. */
    public static boolean handleKey(KeyEvent event) {
        return false;
    }

    /**
     * After {@link LorieView#connect} from {@code app.trierarch.X11Runtime} (Binder fd path), match the
     * same stub / visibility updates as {@link MainActivity#tryConnect()} — the Java activity will not
     * run that code when connect completes only in Kotlin.
     */
    public static void syncLorieConnectionUi() {
        MainActivity m = MainActivity.getInstance();
        if (m != null) {
            m.clientConnectedStateChanged();
        }
    }

    /**
     * Same as upstream: after Lorie is attached, run {@link MainActivity#tryConnect()} (no duplicate
     * [app.trierarch.X11Runtime#tryConnect] — the server is in process {@code :x11}).
     */
    public static void onLorieViewReadyForConnect() {
        MainActivity m = MainActivity.getInstance();
        if (m == null) return;
        MainActivity.handler.post(() -> {
            if (LorieView.connected()) {
                m.clientConnectedStateChanged();
            }
            m.tryConnect();
        });
    }
}
