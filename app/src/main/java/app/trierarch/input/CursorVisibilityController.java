package app.trierarch.input;

/** Keeps a compositor cursor aligned with the active Android pointing device. */
public final class CursorVisibilityController {
    public interface Sink {
        void setCursorVisible(boolean visible);
    }

    private final Sink sink;
    private boolean visible = true;

    public CursorVisibilityController(Sink sink) {
        this.sink = sink;
    }

    public void onPhysicalMouseActivity() {
        setVisible(false);
    }

    public void onTouchActivity() {
        setVisible(true);
    }

    public void reset() {
        setVisible(true);
    }

    private void setVisible(boolean nextVisible) {
        if (visible == nextVisible) return;
        visible = nextVisible;
        sink.setCursorVisible(nextVisible);
    }
}
