package app.trierarch.input;

/** Protocol-independent output of Android pointer handling. */
public interface PointerEventSink {
    enum ScrollSource {
        FINGER,
        WHEEL,
    }

    void moveRelative(float deltaX, float deltaY);
    void moveAbsolute(float x, float y);
    void setButton(int button, boolean pressed);
    void scroll(float deltaX, float deltaY, ScrollSource source);

    default void cancel() { }
}
