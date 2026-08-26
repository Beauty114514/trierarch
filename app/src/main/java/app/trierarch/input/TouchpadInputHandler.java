package app.trierarch.input;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

/** Handles touchpad gestures and translates them into protocol-independent pointer events. */
public final class TouchpadInputHandler implements TapGestureDetector.Listener {
    private final TouchpadInputStrategy strategy;
    private final TapGestureDetector tapDetector;
    private final GestureDetector gestureDetector;

    public TouchpadInputHandler(Context context, PointerEventSink sink) {
        strategy = new TouchpadInputStrategy(sink);
        tapDetector = new TapGestureDetector(context, this);
        gestureDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
            @Override public boolean onDown(MotionEvent event) { return true; }
            @Override public boolean onScroll(MotionEvent first, MotionEvent current,
                    float distanceX, float distanceY) {
                if (current.getPointerCount() == 2) {
                    strategy.onScroll(distanceX, distanceY);
                    return true;
                }
                if (current.getPointerCount() != 1) return false;
                strategy.onPointerMove(distanceX, distanceY);
                return true;
            }
        });
        gestureDetector.setIsLongpressEnabled(false);
    }

    public boolean onTouchEvent(View view, MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_HOVER_EXIT) {
            cancel(view);
            return true;
        }
        strategy.onTouchEvent(event);
        gestureDetector.onTouchEvent(event);
        tapDetector.onTouchEvent(event);
        return true;
    }

    public boolean onGenericMotionEvent(MotionEvent event) {
        if (event.getActionMasked() != MotionEvent.ACTION_SCROLL) return false;
        strategy.onScroll(-100f * event.getAxisValue(MotionEvent.AXIS_HSCROLL),
                -100f * event.getAxisValue(MotionEvent.AXIS_VSCROLL));
        return true;
    }

    public boolean onHoverEvent(View view, MotionEvent event) {
        if (event.getActionMasked() != MotionEvent.ACTION_HOVER_EXIT) return false;
        cancel(view);
        return true;
    }

    public void cancel(View view) {
        long now = android.os.SystemClock.uptimeMillis();
        MotionEvent cancel = MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0f, 0f, 0);
        strategy.onTouchEvent(cancel);
        gestureDetector.onTouchEvent(cancel);
        cancel.recycle();
        tapDetector.cancel();
        strategy.cancel();
    }

    @Override public void onTap(int pointerCount) { strategy.onTap(pointerCount); }
    @Override public void onLongPress(int pointerCount) { strategy.onLongPress(pointerCount); }
}
