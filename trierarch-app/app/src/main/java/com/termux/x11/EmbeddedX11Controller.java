package com.termux.x11;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.view.Display;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import com.termux.x11.input.InputEventSender;
import com.termux.x11.input.InputStub;
import com.termux.x11.input.RenderData;

/**
 * Minimal embedded controller for hosting {@link LorieView} inside Trierarch's Compose activity.
 *
 * <p>Why this lives in {@code com.termux.x11}: we must be able to call package-private APIs
 * (e.g. {@link LorieView#sendWindowChange}) and keep behavior close to upstream.</p>
 *
 * <p>This controller does not create any Activity. It only wires a pre-created {@link LorieView}
 * to the {@code :x11} server process and provides basic input mapping.</p>
 */
public final class EmbeddedX11Controller {
    public enum MouseMode {
        TOUCHPAD,
        TOUCH,
    }

    private final RenderData renderData = new RenderData();
    private final InputEventSender injector;

    private @Nullable Activity hostActivity;
    private @Nullable Context appContext;
    private @Nullable LorieView lorieView;
    private @Nullable ICmdEntryInterface service;

    private @Nullable BroadcastReceiver receiver;
    private boolean registered;

    private MouseMode mouseMode = MouseMode.TOUCHPAD;
    private boolean primaryDown;
    private float lastX, lastY, downX, downY;
    private boolean moved;

    public EmbeddedX11Controller() {
        injector = new InputEventSender(new InputStub() {
            @Override
            public void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative) {
                LorieView lv = lorieView;
                if (lv != null) lv.sendMouseEvent(x, y, whichButton, buttonDown, relative);
            }

            @Override
            public void sendTouchEvent(int action, int id, int x, int y) {
                LorieView lv = lorieView;
                if (lv != null) lv.sendTouchEvent(action, id, x, y);
            }

            @Override
            public void sendStylusEvent(float x, float y, int pressure, int tiltX, int tiltY, int orientation, int buttons, boolean eraser, boolean mouseMode) {
                LorieView lv = lorieView;
                if (lv != null) lv.sendStylusEvent(x, y, pressure, tiltX, tiltY, orientation, buttons, eraser, mouseMode);
            }

            @Override
            public void sendMouseWheelEvent(float deltaX, float deltaY) {
                LorieView lv = lorieView;
                if (lv != null) lv.sendMouseWheelEvent(deltaX, deltaY);
            }

            @Override
            public boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown) {
                LorieView lv = lorieView;
                return lv != null && lv.sendKeyEvent(scanCode, keyCode, keyDown);
            }

            @Override
            public void sendTextEvent(byte[] utf8Bytes) {
                LorieView lv = lorieView;
                if (lv != null) lv.sendTextEvent(utf8Bytes);
            }
        });
    }

    public void setMouseMode(@NonNull MouseMode mode) {
        mouseMode = mode;
    }

    public void attach(@NonNull Activity activity, @NonNull ViewGroup container, @NonNull LorieView view) {
        detach();
        hostActivity = activity;
        appContext = activity.getApplicationContext();
        lorieView = view;
        X11ActivityBridge.setHostActivity(activity);
        X11ActivityBridge.setLorieView(view);

        // Keep screen sizing in sync (prevents stretch).
        view.setCallback((surfaceWidth, surfaceHeight, screenWidth, screenHeight) -> {
            int framerate;
            try {
                framerate = (int) ((view.getDisplay() != null) ? view.getDisplay().getRefreshRate() : 30);
            } catch (Throwable t) {
                framerate = 30;
            }
            String name;
            try {
                if (view.getDisplay() == null || view.getDisplay().getDisplayId() == Display.DEFAULT_DISPLAY) name = "builtin";
                else name = "external";
            } catch (Throwable t) {
                name = "builtin";
            }
            // Package-private: only callable inside com.termux.x11.
            LorieView.sendWindowChange(screenWidth, screenHeight, framerate, name);

            int vw = view.getWidth() > 0 ? view.getWidth() : surfaceWidth;
            int vh = view.getHeight() > 0 ? view.getHeight() : surfaceHeight;
            renderData.imageWidth = vw;
            renderData.imageHeight = vh;
            renderData.screenWidth = screenWidth;
            renderData.screenHeight = screenHeight;
            float sx = vw > 0 ? (float) screenWidth / (float) vw : 1f;
            float sy = vh > 0 ? (float) screenHeight / (float) vh : 1f;
            renderData.scale.set(sx, sy);
        });

        // Hide system cursor (upstream behavior).
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                view.setPointerIcon(PointerIcon.getSystemIcon(activity, PointerIcon.TYPE_NULL));
            }
        } catch (Throwable ignored) {}

        // Bind listeners to the LorieView itself to avoid letterbox offsets.
        view.setOnGenericMotionListener((v, e) -> {
            if (e.getActionMasked() == MotionEvent.ACTION_SCROLL) {
                float scrollY = -100f * e.getAxisValue(MotionEvent.AXIS_VSCROLL);
                float scrollX = -100f * e.getAxisValue(MotionEvent.AXIS_HSCROLL);
                view.sendMouseWheelEvent(scrollX, scrollY);
                return true;
            }
            return false;
        });

        view.setOnTouchListener((v, e) -> {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                view.requestFocus();
            }

            if (mouseMode == MouseMode.TOUCH) {
                injector.sendTouchEvent(e, renderData);
                return true;
            }

            // TOUCHPAD (original-like mapping, relative movement).
            int slop = ViewConfiguration.get(activity).getScaledTouchSlop();
            int slopSq = slop * slop;
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downX = lastX = e.getX();
                    downY = lastY = e.getY();
                    moved = false;
                    primaryDown = false;
                    return true;
                case MotionEvent.ACTION_POINTER_DOWN:
                    downX = lastX = e.getX(0);
                    downY = lastY = e.getY(0);
                    moved = false;
                    primaryDown = false;
                    return true;
                case MotionEvent.ACTION_MOVE: {
                    if (e.getPointerCount() >= 2) {
                        // Two-finger scroll.
                        float dx = (e.getX(0) - lastX);
                        float dy = (e.getY(0) - lastY);
                        lastX = e.getX(0);
                        lastY = e.getY(0);
                        view.sendMouseWheelEvent(-dx, -dy);
                        moved = true;
                        return true;
                    }
                    float dx = e.getX() - lastX;
                    float dy = e.getY() - lastY;
                    lastX = e.getX();
                    lastY = e.getY();

                    float totalDx = e.getX() - downX;
                    float totalDy = e.getY() - downY;
                    if (!moved && (totalDx * totalDx + totalDy * totalDy) > slopSq) {
                        moved = true;
                        primaryDown = true;
                        view.sendMouseEvent(0f, 0f, InputStub.BUTTON_LEFT, true, true);
                    }
                    float rx = dx * renderData.scale.x;
                    float ry = dy * renderData.scale.y;
                    view.sendMouseEvent(rx, ry, InputStub.BUTTON_UNDEFINED, false, true);
                    return true;
                }
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                case MotionEvent.ACTION_POINTER_UP: {
                    if (primaryDown) {
                        primaryDown = false;
                        view.sendMouseEvent(0f, 0f, InputStub.BUTTON_LEFT, false, true);
                        return true;
                    }
                    if (!moved) {
                        int button = InputStub.BUTTON_LEFT;
                        int pointers = Math.min(e.getPointerCount(), 3);
                        if (pointers == 2) button = InputStub.BUTTON_RIGHT;
                        else if (pointers == 3) button = InputStub.BUTTON_MIDDLE;
                        view.sendMouseEvent(0f, 0f, button, true, true);
                        view.sendMouseEvent(0f, 0f, button, false, true);
                    }
                    moved = false;
                    return true;
                }
            }
            return false;
        });

        registerReceiverIfNeeded();
        // Nudge CmdEntryPoint to broadcast connection details again.
        try { LorieView.requestConnection(); } catch (Throwable ignored) {}
    }

    public void detach() {
        if (registered && receiver != null && appContext != null) {
            try { appContext.unregisterReceiver(receiver); } catch (Throwable ignored) {}
        }
        registered = false;
        receiver = null;
        service = null;
        lorieView = null;
        appContext = null;
        if (X11ActivityBridge.getHostActivity() == hostActivity) {
            X11ActivityBridge.setHostActivity(null);
        }
        X11ActivityBridge.setLorieView(null);
        hostActivity = null;
        primaryDown = false;
        moved = false;
    }

    private void registerReceiverIfNeeded() {
        if (registered) return;
        final Context ctx = appContext;
        if (ctx == null) return;
        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent == null) return;
                if (!CmdEntryPoint.ACTION_START.equals(intent.getAction())) return;
                BundleCompat bundleCompat = BundleCompat.fromIntent(intent);
                IBinder b = bundleCompat != null ? bundleCompat.binder : null;
                if (b == null) return;
                onReceiveConnection(b);
            }
        };
        IntentFilter filter = new IntentFilter(CmdEntryPoint.ACTION_START);
        ContextCompat.registerReceiver(ctx, receiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED);
        registered = true;
    }

    private void onReceiveConnection(IBinder binder) {
        service = ICmdEntryInterface.Stub.asInterface(binder);
        try {
            service.asBinder().linkToDeath(() -> {
                service = null;
                try { LorieView.requestConnection(); } catch (Throwable ignored) {}
            }, 0);
        } catch (Throwable ignored) {}

        try {
            ParcelFileDescriptor logcat = service.getLogcatOutput();
            if (logcat != null) LorieView.startLogcat(logcat.detachFd());
        } catch (Throwable ignored) {}

        tryConnect();
    }

    private void tryConnect() {
        if (LorieView.connected()) return;
        if (service == null) {
            try { LorieView.requestConnection(); } catch (Throwable ignored) {}
            return;
        }
        try {
            ParcelFileDescriptor fd = service.getXConnection();
            if (fd != null) {
                LorieView.connect(fd.detachFd());
                Prefs p = X11ActivityBridge.getOrCreatePrefs(appContext);
                if (lorieView != null) {
                    lorieView.reloadPreferences(p);
                    lorieView.triggerCallback();
                }
            }
        } catch (Throwable t) {
            service = null;
        }
    }

    /**
     * Extracts a binder from the weird "null key" bundle used by CmdEntryPoint.
     * Kept here to avoid Kotlin reflection gymnastics.
     */
    private static final class BundleCompat {
        final IBinder binder;
        private BundleCompat(IBinder binder) { this.binder = binder; }

        static @Nullable BundleCompat fromIntent(@NonNull Intent intent) {
            try {
                android.os.Bundle b = intent.getBundleExtra(null);
                if (b == null) return null;
                IBinder binder = b.getBinder(null);
                return binder != null ? new BundleCompat(binder) : null;
            } catch (Throwable t) {
                return null;
            }
        }
    }
}

