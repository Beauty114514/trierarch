package com.termux.x11.input;

/** Keeps the rendered X11 cursor aligned with the active Android input device. */
public final class CursorVisibilityController {
    public interface Sink {
        void setCursorVisible(boolean visible);
    }

    private final Sink sink;
    private boolean visible = true;

    public CursorVisibilityController(Sink sink) {
        this.sink = sink;
    }

    public void onPhysicalMouseActivity() { setVisible(false); }
    public void onTouchActivity() { setVisible(true); }
    public void reset() { setVisible(true); }

    private void setVisible(boolean next) {
        if (visible == next) return;
        visible = next;
        sink.setCursorVisible(next);
    }
}
