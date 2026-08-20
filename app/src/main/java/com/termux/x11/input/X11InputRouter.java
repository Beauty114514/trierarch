package com.termux.x11.input;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

/** Routes Android pointer events to the physical-mouse or touchpad handler. */
public final class X11InputRouter {
    private final PhysicalMouseInputHandler physicalMouseHandler;
    private final TouchpadInputHandler touchpadHandler;
    private final CursorVisibilityController cursorVisibilityController;

    public X11InputRouter(Context context, InputEventSender sender) {
        this(context, sender, visible -> { });
    }

    public X11InputRouter(
            Context context, InputEventSender sender, CursorVisibilityController.Sink cursorSink) {
        physicalMouseHandler = new PhysicalMouseInputHandler(sender);
        touchpadHandler = new TouchpadInputHandler(context, sender);
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
        cursorVisibilityController.reset();
    }
}
