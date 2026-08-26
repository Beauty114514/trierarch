package app.trierarch.input;

import android.os.Build;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

/** Recognizes and forwards physical mouse events without applying touch gestures. */
public final class PhysicalMouseInputHandler {
    private static final int[][] BUTTONS = {
            {MotionEvent.BUTTON_PRIMARY, 1},
            {MotionEvent.BUTTON_TERTIARY, 2},
            {MotionEvent.BUTTON_SECONDARY, 3}
    };

    private final PointerEventSink sink;
    private int lastButtonState;

    public PhysicalMouseInputHandler(PointerEventSink sink) {
        this.sink = sink;
    }

    public boolean accepts(MotionEvent event) {
        int index = event.getActionIndex();
        if (index < 0 || index >= event.getPointerCount()) index = 0;
        int tool = event.getPointerCount() == 0
                ? MotionEvent.TOOL_TYPE_UNKNOWN : event.getToolType(index);
        int source = event.getSource();
        return tool == MotionEvent.TOOL_TYPE_MOUSE
                || ((source & InputDevice.SOURCE_MOUSE) != 0
                && tool != MotionEvent.TOOL_TYPE_FINGER
                && (source & InputDevice.SOURCE_TOUCHPAD) == 0);
    }

    public boolean onPointerEvent(View view, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_SCROLL:
                sink.scroll(-100f * event.getAxisValue(MotionEvent.AXIS_HSCROLL),
                        -100f * event.getAxisValue(MotionEvent.AXIS_VSCROLL),
                        PointerEventSink.ScrollSource.WHEEL);
                syncButtons(event);
                return true;
            case MotionEvent.ACTION_HOVER_MOVE:
            case MotionEvent.ACTION_MOVE:
                sendMotion(view, event);
                syncButtons(event);
                return true;
            case MotionEvent.ACTION_BUTTON_PRESS:
            case MotionEvent.ACTION_BUTTON_RELEASE:
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_UP:
                syncButtons(event);
                return true;
            case MotionEvent.ACTION_HOVER_EXIT:
            case MotionEvent.ACTION_CANCEL:
                releaseButtons();
                return true;
            default:
                syncButtons(event);
                return true;
        }
    }

    public void cancel() {
        releaseButtons();
    }

    private void sendMotion(View view, MotionEvent event) {
        InputDevice device = event.getDevice();
        boolean captured = view != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && view.hasPointerCapture();
        boolean relativeSource = (event.getSource() & InputDevice.SOURCE_MOUSE_RELATIVE)
                == InputDevice.SOURCE_MOUSE_RELATIVE;
        boolean relativeAxes = device != null
                && device.getMotionRange(MotionEvent.AXIS_RELATIVE_X) != null;
        if (captured && (relativeSource || relativeAxes)) {
            sink.moveRelative(relativeAxes ? event.getAxisValue(MotionEvent.AXIS_RELATIVE_X) : event.getX(),
                    relativeAxes ? event.getAxisValue(MotionEvent.AXIS_RELATIVE_Y) : event.getY());
        } else if (!captured) {
            sink.moveAbsolute(event.getX(), event.getY());
        }
    }

    private void syncButtons(MotionEvent event) {
        int current = event.getButtonState();
        for (int[] button : BUTTONS) {
            int mask = button[0];
            if ((lastButtonState & mask) != (current & mask))
                sink.setButton(button[1], (current & mask) != 0);
        }
        lastButtonState = current;
    }

    private void releaseButtons() {
        if (lastButtonState == 0) return;
        for (int[] button : BUTTONS) {
            if ((lastButtonState & button[0]) != 0) sink.setButton(button[1], false);
        }
        lastButtonState = 0;
    }
}
