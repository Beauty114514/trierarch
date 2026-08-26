package app.trierarch.input;

/** Protocol-independent output of Android pointer handling. */
public interface PointerEventSink {
    void moveRelative(float deltaX, float deltaY);
    void moveAbsolute(float x, float y);
    void setButton(int button, boolean pressed);
    void scroll(float deltaX, float deltaY);

    default void cancel() { }
}
