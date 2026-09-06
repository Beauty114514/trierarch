package com.termux.x11;

import android.content.Context;
import android.os.SystemClock;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.KeyEvent;

import androidx.annotation.Keep;

import app.trierarch.input.PointerInputRouter;
import app.trierarch.input.PhysicalKeyEvent;
import app.trierarch.input.PhysicalKeyboardRouter;
import com.termux.x11.input.X11PointerEventSink;

import dalvik.annotation.optimization.CriticalNative;
import dalvik.annotation.optimization.FastNative;

/**
 * A deliberately small SurfaceView facade for the fixed Lorie JNI ABI.
 *
 * It owns rendering and the X11 pointer input path. Keyboard,
 * clipboard, scaling controls and external-display policy remain separate.
 */
@Keep
public final class LorieView extends SurfaceView {
    private long nativeHandle;
    private int latestWidth;
    private int latestHeight;
    private final PointerInputRouter inputRouter;
    private final PhysicalKeyboardRouter keyboardRouter;

    public LorieView(Context context) {
        super(context);
        inputRouter = new PointerInputRouter(context,
                new X11PointerEventSink((x, y, button, down, relative) ->
                        sendMouseEvent(nativeHandle, x, y, button, down, relative)),
                this::setCursorVisible);
        keyboardRouter = new PhysicalKeyboardRouter(event -> {
            if (nativeHandle == 0 || !isConnected()) return false;
            return sendKeyEvent(
                    nativeHandle,
                    event.getScanCode(),
                    event.getKeyCode(),
                    event.getAction() == PhysicalKeyEvent.Action.DOWN
            );
        });
        nativeHandle = nativeInit();
        setFocusable(true);
        setFocusableInTouchMode(true);
        getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override public void surfaceCreated(SurfaceHolder holder) {
                holder.setFormat(5); // HAL_PIXEL_FORMAT_BGRA_8888
            }

            @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                latestWidth = width;
                latestHeight = height;
                if (nativeHandle != 0) {
                    LorieView.this.surfaceChanged(nativeHandle, holder.getSurface());
                    publishSize();
                }
            }

            @Override public void surfaceDestroyed(SurfaceHolder holder) {
                if (nativeHandle != 0) LorieView.this.surfaceChanged(nativeHandle, null);
            }
        });
    }

    public void attachConnection(int fileDescriptor) {
        if (nativeHandle == 0) return;
        connect(nativeHandle, fileDescriptor);
        publishSize();
    }

    public boolean isConnected() {
        return nativeHandle != 0 && connected(nativeHandle);
    }

    public void setCursorVisible(boolean visible) {
        if (nativeHandle != 0) setCursorVisible(nativeHandle, visible);
    }

    /** Releases external keys before this view stops being the desktop target. */
    public void releasePressedKeys() {
        keyboardRouter.releaseAll(SystemClock.uptimeMillis());
    }

    @Override public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyboardRouter.dispatchAndroidEvent(event)) return true;
        return super.onKeyDown(keyCode, event);
    }

    @Override public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyboardRouter.dispatchAndroidEvent(event)) return true;
        return super.onKeyUp(keyCode, event);
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (nativeHandle == 0) return true;
        return inputRouter.onTouchEvent(this, event);
    }

    @Override public boolean onGenericMotionEvent(MotionEvent event) {
        if (nativeHandle == 0) return true;
        return inputRouter.onGenericMotionEvent(this, event) || super.onGenericMotionEvent(event);
    }

    @Override public boolean onHoverEvent(MotionEvent event) {
        if (nativeHandle == 0) return true;
        return inputRouter.onHoverEvent(this, event) || super.onHoverEvent(event);
    }

    @Override protected void onDetachedFromWindow() {
        inputRouter.cancel(this);
        releasePressedKeys();
        if (nativeHandle != 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
        }
        super.onDetachedFromWindow();
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) {
            inputRouter.cancel(this);
            releasePressedKeys();
        }
    }

    @Override protected void onFocusChanged(boolean gainFocus, int direction, android.graphics.Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        if (!gainFocus) releasePressedKeys();
    }

    private void publishSize() {
        if (nativeHandle == 0 || latestWidth <= 0 || latestHeight <= 0) return;
        setViewport(nativeHandle, 0, 0, latestWidth, latestHeight, latestWidth, latestHeight, 0);
        sendWindowChange(nativeHandle, latestWidth, latestHeight, refreshRate(), "builtin");
    }

    private int refreshRate() {
        return getDisplay() == null ? 60 : Math.max(1, Math.round(getDisplay().getRefreshRate()));
    }

    // Called by Lorie's renderer. Trierarch currently renders at the SurfaceView bounds.
    @Keep public static void setRendererViewport(
        int x, int y, int width, int height, float left, float top, float sourceWidth, float sourceHeight
    ) {}

    // Called by Lorie's native event loop; clipboard is intentionally deferred.
    @Keep void setClipboardText(String text) {}
    @Keep void requestClipboard() {}
    @Keep void resetIme() {}

    @FastNative private native long nativeInit();
    @FastNative private native void nativeDestroy(long handle);
    @FastNative private native void surfaceChanged(long handle, Surface surface);
    @FastNative private native void setViewport(long handle, int x, int y, int width, int height, int expectedWidth, int expectedHeight, int hidden);
    @FastNative private native void setCursorVisible(long handle, boolean visible);
    @FastNative private native void setRendererZoom(long handle, int percent);
    @FastNative private native void setFiltering(long handle, int filtering);
    @FastNative private static native void connect(long handle, int fileDescriptor);
    @CriticalNative private static native boolean connected(long handle);
    @FastNative private static native void startLogcat(long handle, int fileDescriptor);
    @FastNative private static native void setClipboardSyncEnabled(long handle, boolean enabled, boolean ignored);
    @FastNative private native void sendClipboardAnnounce(long handle);
    @FastNative private native void sendClipboardEvent(long handle, byte[] text);
    @FastNative private static native void sendWindowChange(long handle, int width, int height, int framerate, String name);
    @FastNative private native void sendMouseEvent(long handle, float x, float y, int button, boolean down, boolean relative);
    @FastNative private native void sendTouchEvent(long handle, int action, int id, int x, int y);
    @FastNative private native void sendStylusEvent(long handle, float x, float y, int pressure, int tiltX, int tiltY, int orientation, int buttons, boolean eraser, boolean mouse);
    @FastNative private static native void requestStylusEnabled(long handle, boolean enabled);
    @FastNative private static native void sendLockKeysState(long handle, int state);
    @FastNative private native boolean sendKeyEvent(long handle, int scanCode, int keyCode, boolean down);
    @FastNative private native void sendTextEvent(long handle, byte[] text);
    @CriticalNative private static native boolean requestConnection(long handle);
    @FastNative private static native long getLastInputTimestamp();
    @FastNative private static native void markUserActivity();

    static {
        System.loadLibrary("Xlorie");
    }
}
