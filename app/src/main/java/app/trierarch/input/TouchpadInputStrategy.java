package app.trierarch.input;

import android.view.MotionEvent;

/** Translates touchpad gesture callbacks into protocol-independent pointer events. */
final class TouchpadInputStrategy {
    private final PointerEventSink sink;
    private boolean buttonHeld;
    private int heldButton;

    TouchpadInputStrategy(PointerEventSink sink) {
        this.sink = sink;
    }

    void onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                releaseHeldButton();
                break;
            default:
                break;
        }
    }

    void onPointerMove(float distanceX, float distanceY) {
        sink.moveRelative(-distanceX, -distanceY);
    }

    void onScroll(float distanceX, float distanceY) {
        sink.scroll(distanceX, distanceY);
    }

    void onTap(int pointerCount) {
        int button = buttonForPointerCount(pointerCount);
        sink.setButton(button, true);
        sink.setButton(button, false);
    }

    void onLongPress(int pointerCount) {
        int button = buttonForPointerCount(pointerCount);
        releaseHeldButton();
        sink.setButton(button, true);
        heldButton = button;
        buttonHeld = true;
    }

    void cancel() {
        releaseHeldButton();
    }

    private static int buttonForPointerCount(int pointerCount) {
        if (pointerCount >= 3) return 2;
        return pointerCount == 2 ? 3 : 1;
    }

    private void releaseHeldButton() {
        if (!buttonHeld) return;
        sink.setButton(heldButton, false);
        heldButton = 0;
        buttonHeld = false;
    }
}
