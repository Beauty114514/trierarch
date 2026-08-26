package app.trierarch.input;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

/** Routes Android events to physical-mouse or simulated-touchpad handling. */
public final class PointerInputRouter {
    private final PointerEventSink sink;
    private final PhysicalMouseInputHandler physicalMouseHandler;
    private final TouchpadInputHandler touchpadHandler;
    private final CursorVisibilityController cursorVisibilityController;

    public PointerInputRouter(Context context, PointerEventSink sink,
            CursorVisibilityController.Sink cursorSink) {
        this.sink = sink;
        physicalMouseHandler = new PhysicalMouseInputHandler(sink);
        touchpadHandler = new TouchpadInputHandler(context, sink);
        cursorVisibilityController = new CursorVisibilityController(cursorSink);
    }

    public boolean onTouchEvent(View view, MotionEvent event) {
        if (physicalMouseHandler.accepts(event)) {
            cursorVisibilityController.onPhysicalMouseActivity();
            return physicalMouseHandler.onPointerEvent(view, event);
        }
        cursorVisibilityController.onTouchActivity();
        return touchpadHandler.onTouchEvent(view, event);
    }

    public boolean onGenericMotionEvent(View view, MotionEvent event) {
        if (physicalMouseHandler.accepts(event)) {
            cursorVisibilityController.onPhysicalMouseActivity();
            return physicalMouseHandler.onPointerEvent(view, event);
        }
        cursorVisibilityController.onTouchActivity();
        return touchpadHandler.onGenericMotionEvent(event);
    }

    public boolean onHoverEvent(View view, MotionEvent event) {
        if (physicalMouseHandler.accepts(event)) {
            cursorVisibilityController.onPhysicalMouseActivity();
            return physicalMouseHandler.onPointerEvent(view, event);
        }
        cursorVisibilityController.onTouchActivity();
        return touchpadHandler.onHoverEvent(view, event);
    }

    public void cancel(View view) {
        touchpadHandler.cancel(view);
        physicalMouseHandler.cancel();
        sink.cancel();
        cursorVisibilityController.reset();
    }
}
