package com.termux.x11.input;

import android.content.Context;
import android.graphics.PointF;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

/** Detects multi-finger taps and long presses with explicit pointer tracking. */
public final class TapGestureDetector {
    public interface Listener {
        void onTap(int pointerCount);
        void onLongPress(int pointerCount);
    }

    private final Listener listener;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final SparseArray<PointF> initialPositions = new SparseArray<>();
    private final int touchSlopSquare;

    private int pointerCount;
    private PointF initialPoint;
    private boolean tapCancelled;

    private final Runnable longPress;

    public TapGestureDetector(Context context, Listener listener) {
        this.listener = listener;
        longPress = () -> {
            if (!tapCancelled && pointerCount > 0 && initialPoint != null) {
                tapCancelled = true;
                this.listener.onLongPress(pointerCount);
                initialPoint = null;
            }
        };
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        touchSlopSquare = touchSlop * touchSlop;
    }

    public void onTouchEvent(View owner, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                reset();
                trackDownEvent(event);
                pointerCount = 1;
                handler.postDelayed(longPress, ViewConfiguration.getLongPressTimeout());
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                trackDownEvent(event);
                pointerCount = Math.max(pointerCount, event.getPointerCount());
                break;
            case MotionEvent.ACTION_MOVE:
                if (!tapCancelled && trackMoveEvent(event)) {
                    cancelLongPress();
                    tapCancelled = true;
                }
                break;
            case MotionEvent.ACTION_POINTER_UP:
                cancelLongPress();
                trackUpEvent(event);
                break;
            case MotionEvent.ACTION_UP:
                cancelLongPress();
                if (!tapCancelled && initialPoint != null && pointerCount > 0) {
                    listener.onTap(pointerCount);
                }
                reset();
                break;
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_HOVER_EXIT:
                reset();
                break;
            default:
                break;
        }
    }

    public boolean hasMoved() {
        return tapCancelled;
    }

    public void cancel(View owner) {
        reset();
    }

    private void trackDownEvent(MotionEvent event) {
        int index = event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN
                ? event.getActionIndex() : 0;
        int pointerId = event.getPointerId(index);
        PointF position = new PointF(event.getX(index), event.getY(index));
        initialPositions.put(pointerId, position);
        if (initialPoint == null) initialPoint = position;
    }

    private void trackUpEvent(MotionEvent event) {
        int index = event.getActionMasked() == MotionEvent.ACTION_POINTER_UP
                ? event.getActionIndex() : 0;
        initialPositions.remove(event.getPointerId(index));
    }

    private boolean trackMoveEvent(MotionEvent event) {
        for (int index = 0; index < event.getPointerCount(); index++) {
            int pointerId = event.getPointerId(index);
            PointF down = initialPositions.get(pointerId);
            if (down == null) {
                initialPositions.put(pointerId, new PointF(event.getX(index), event.getY(index)));
                continue;
            }
            float dx = event.getX(index) - down.x;
            float dy = event.getY(index) - down.y;
            if (dx * dx + dy * dy > touchSlopSquare) return true;
        }
        return false;
    }

    private void reset() {
        cancelLongPress();
        pointerCount = 0;
        initialPositions.clear();
        initialPoint = null;
        tapCancelled = false;
    }

    private void cancelLongPress() {
        handler.removeCallbacks(longPress);
    }
}
