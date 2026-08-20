package com.termux.x11.input;

import android.view.MotionEvent;

/** Translates gesture callbacks into relative X11 mouse events. */
public final class TrackpadInputStrategy implements TapGestureDetector.Listener {
    private final InputEventSender sender;
    private boolean buttonHeld;
    private int heldButton;

    public TrackpadInputStrategy(InputEventSender sender) {
        this.sender = sender;
    }

    public void onMotionEvent(MotionEvent event) {
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

    public void onCursorMove(float distanceX, float distanceY) {
        sender.sendCursorMove(-distanceX, -distanceY, true);
    }

    public void onScroll(float distanceX, float distanceY) {
        sender.sendMouseWheelEvent(distanceX, distanceY);
    }

    @Override public void onTap(int pointerCount) {
        sender.sendMouseClick(buttonForPointerCount(pointerCount), true);
    }

    @Override public void onLongPress(int pointerCount) {
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
