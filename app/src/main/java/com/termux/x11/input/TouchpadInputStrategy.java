package com.termux.x11.input;

import android.view.MotionEvent;

/** Translates touchpad gesture callbacks into relative X11 pointer events. */
public final class TouchpadInputStrategy {
    private final InputEventSender sender;
    private boolean buttonHeld;
    private int heldButton;

    public TouchpadInputStrategy(InputEventSender sender) {
        this.sender = sender;
    }

    public void onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_HOVER_EXIT:
                releaseHeldButton();
                break;
            default:
                break;
        }
    }

    public void onPointerMove(float distanceX, float distanceY) {
        sender.sendPointerMove(-distanceX, -distanceY, true);
    }

    public void onScroll(float distanceX, float distanceY) {
        sender.sendMouseWheelEvent(distanceX, distanceY);
    }

    public void onTap(int pointerCount) {
        sender.sendMouseClick(buttonForPointerCount(pointerCount), true);
    }

    public void onLongPress(int pointerCount) {
        int button = buttonForPointerCount(pointerCount);
        releaseHeldButton();
        sender.sendMouseDown(button, true);
        heldButton = button;
        buttonHeld = true;
    }

    public void cancel() {
        releaseHeldButton();
    }

    private int buttonForPointerCount(int pointerCount) {
        if (pointerCount >= 3) return 2;
        return pointerCount == 2 ? 3 : 1;
    }

    private void releaseHeldButton() {
        if (!buttonHeld) return;
        sender.sendMouseUp(heldButton, true);
        heldButton = 0;
        buttonHeld = false;
    }
}
