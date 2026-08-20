package com.termux.x11.input;

/** Small event boundary between Android gesture handling and the X11 JNI ABI. */
public final class InputEventSender {
    public interface Stub {
        void sendMouseEvent(float x, float y, int button, boolean down, boolean relative);
    }

    private final Stub stub;

    public InputEventSender(Stub stub) {
        this.stub = stub;
    }

    public void sendMouseDown(int button, boolean relative) {
        stub.sendMouseEvent(0f, 0f, button, true, relative);
    }

    public void sendMouseUp(int button, boolean relative) {
        stub.sendMouseEvent(0f, 0f, button, false, relative);
    }

    public void sendMouseClick(int button, boolean relative) {
        sendMouseDown(button, relative);
        sendMouseUp(button, relative);
    }

    public void sendCursorMove(float x, float y, boolean relative) {
        if (x != 0f || y != 0f) stub.sendMouseEvent(x, y, 0, false, relative);
    }

    public void sendMouseWheelEvent(float distanceX, float distanceY) {
        // Lorie reserves button 4 as a valuator event. Native code converts
        // x/y distance into the X11 horizontal/vertical scroll valuators.
        stub.sendMouseEvent(distanceX, distanceY, 4, false, true);
    }
}
