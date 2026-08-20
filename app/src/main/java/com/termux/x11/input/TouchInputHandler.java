package com.termux.x11.input;

import android.content.Context;
import android.os.SystemClock;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

/** Owns the Android event pipeline used by the X11 touchpad mode. */
public final class TouchInputHandler implements TapGestureDetector.Listener {
    private final TrackpadInputStrategy strategy;
    private final TapGestureDetector taps;
    private final GestureDetector scroller;

    public TouchInputHandler(Context context, InputEventSender sender) {
        strategy = new TrackpadInputStrategy(sender);
        taps = new TapGestureDetector(context, this);
        scroller = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
            @Override public boolean onDown(MotionEvent event) {
                return true;
            }

            @Override public boolean onScroll(
                    MotionEvent first, MotionEvent current, float distanceX, float distanceY) {
                if (current.getPointerCount() == 2) {
                    strategy.onScroll(distanceX, distanceY);
                    return true;
                }
                if (current.getPointerCount() != 1) return false;
                strategy.onCursorMove(distanceX, distanceY);
                return true;
            }
        });
        scroller.setIsLongpressEnabled(false);
    }

    public boolean onTouchEvent(View owner, MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_HOVER_EXIT) {
            cancel(owner);
            return true;
        }

        // Match Termux:X11: the strategy observes every original event first,
        // then all gesture detectors receive the same event unconditionally.
        strategy.onMotionEvent(event);
        scroller.onTouchEvent(event);
        taps.onTouchEvent(owner, event);
        return true;
    }

    public boolean onGenericMotionEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_SCROLL) {
            strategy.onScroll(
                    -100f * event.getAxisValue(MotionEvent.AXIS_HSCROLL),
                    -100f * event.getAxisValue(MotionEvent.AXIS_VSCROLL));
            return true;
        }
        return false;
    }

    public boolean onHoverEvent(View owner, MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_HOVER_EXIT) {
            cancel(owner);
            return true;
        }
        return false;
    }

    @Override public void onTap(int pointerCount) {
        strategy.onTap(pointerCount);
    }

    @Override public void onLongPress(int pointerCount) {
        strategy.onLongPress(pointerCount);
    }

    public void cancel(View owner) {
        long now = SystemClock.uptimeMillis();
        MotionEvent cancel = MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0f, 0f, 0);
        strategy.onMotionEvent(cancel);
        scroller.onTouchEvent(cancel);
        cancel.recycle();
        taps.cancel(owner);
        strategy.cancel();
    }
}
