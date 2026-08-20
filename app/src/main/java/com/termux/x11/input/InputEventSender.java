package com.termux.x11.input;

/** Small event boundary between Android gesture handling and the X11 JNI ABI. */
public final class InputEventSender {
    public interface MouseEventSink {
        void sendMouseEvent(float x, float y, int button, boolean down, boolean relative);
    }

    private final MouseEventSink sink;

    public InputEventSender(MouseEventSink sink) {
        this.sink = sink;
    }

    public void sendMouseDown(int button, boolean relative) {
        sink.sendMouseEvent(0f, 0f, button, true, relative);
    }

    public void sendMouseUp(int button, boolean relative) {
        sink.sendMouseEvent(0f, 0f, button, false, relative);
    }

    public void sendMouseButton(int button, boolean down, boolean relative) {
        sink.sendMouseEvent(0f, 0f, button, down, relative);
    }

    public void sendMouseClick(int button, boolean relative) {
        sendMouseDown(button, relative);
        sendMouseUp(button, relative);
    }

    public void sendPointerMove(float x, float y, boolean relative) {
        if (!relative || x != 0f || y != 0f) sink.sendMouseEvent(x, y, 0, false, relative);
    }

    public void sendMouseWheelEvent(float distanceX, float distanceY) {
        // Lorie reserves button 4 as a valuator event. Native code converts
        // x/y distance into the X11 horizontal/vertical scroll valuators.
        sink.sendMouseEvent(distanceX, distanceY, 4, false, true);
    }
}
