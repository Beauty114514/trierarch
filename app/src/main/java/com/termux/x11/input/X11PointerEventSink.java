package com.termux.x11.input;

import app.trierarch.input.PointerEventSink;

/** Adapts protocol-independent pointer events to Lorie's fixed X11 JNI ABI. */
public final class X11PointerEventSink implements PointerEventSink {
    public interface MouseEventSink {
        void sendMouseEvent(float x, float y, int button, boolean down, boolean relative);
    }

    private final MouseEventSink sink;

    public X11PointerEventSink(MouseEventSink sink) {
        this.sink = sink;
    }

    @Override public void moveRelative(float deltaX, float deltaY) {
        if (deltaX != 0f || deltaY != 0f) sink.sendMouseEvent(deltaX, deltaY, 0, false, true);
    }

    @Override public void moveAbsolute(float x, float y) {
        sink.sendMouseEvent(x, y, 0, false, false);
    }

    @Override public void setButton(int button, boolean pressed) {
        sink.sendMouseEvent(0f, 0f, button, pressed, true);
    }

    @Override public void scroll(float deltaX, float deltaY) {
        sink.sendMouseEvent(deltaX, deltaY, 4, false, true);
    }
}
