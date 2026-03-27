package app.trierarch

import android.view.Surface

/**
 * Kotlin/JNI bridge to the in-app Wayland compositor (`libwayland-compositor.so`).
 *
 * Contract:
 * - The compositor runs inside the app process. Kotlin drives it via JNI.
 * - Start order is important: spawn proot first (clients live there), then start the server,
 *   then create/destroy the render surface as the UI shows/hides the Wayland view.
 * - Input events may come from multiple Android sources (touch, mouse, IME, hardware keyboard);
 *   native code is responsible for any cross-thread marshaling needed by libwayland-server.
 */
object WaylandBridge {

    /** Pointer event actions for [nativeOnPointerEvent]. */
    const val POINTER_ACTION_DOWN = 0
    const val POINTER_ACTION_MOVE = 1
    const val POINTER_ACTION_UP = 2
    /** Hover move: (x,y) = absolute cursor position (touchpad mode). */
    const val POINTER_ACTION_POINTER_MOVE = 6

    /** Axis source for [nativeOnPointerAxis]: two-finger scroll (wl_pointer.axis_source.finger). */
    const val AXIS_SOURCE_FINGER = 1

    init {
        try {
            System.loadLibrary("wayland-compositor")
        } catch (e: UnsatisfiedLinkError) {
            // Library not present or deps missing; callers should treat JNI methods as unavailable.
        }
    }

    /**
     * Start the Wayland server and its dispatch thread.
     *
     * Must be called after proot is spawned (Wayland clients live inside the proot environment),
     * and before any Surface is created.
     */
    external fun nativeStartServer(runtimeDir: String)

    /** Create EGL and render thread; call when showing the Wayland view with a valid Surface. */
    external fun nativeSurfaceCreated(surface: Surface, runtimeDir: String, resolutionPercent: Int, scalePercent: Int)

    /** Stop render thread, resume dispatch thread. Call when leaving the Wayland view. */
    external fun nativeSurfaceDestroyed()

    /** Update compositor output size (e.g. on fold/unfold). Call from SurfaceHolder.Callback.surfaceChanged. */
    external fun nativeOutputSizeChanged(width: Int, height: Int, resolutionPercent: Int, scalePercent: Int)

    /**
     * Deliver pointer/touch event to the compositor.
     * @param x surface x (tablet mode) or cursor x (touchpad mode action 6)
     * @param y surface y (tablet mode) or cursor y (touchpad mode action 6)
     * @param action POINTER_ACTION_DOWN(0), MOVE(1), UP(2), POINTER_MOVE(6)
     * @param timeMs event time in milliseconds
     */
    external fun nativeOnPointerEvent(x: Float, y: Float, action: Int, timeMs: Int)

    /**
     * Deliver pointer axis (scroll) to the focused client. Used for two-finger swipe → scroll.
     * @param deltaX horizontal scroll delta (e.g. finger movement in X)
     * @param deltaY vertical scroll delta (e.g. finger movement in Y; positive = scroll down)
     * @param timeMs event time in milliseconds
     * @param axisSource AXIS_SOURCE_FINGER (1) for touch, or 0 for wheel
     */
    external fun nativeOnPointerAxis(deltaX: Float, deltaY: Float, timeMs: Int, axisSource: Int)

    /** Send right button click (down+up) at (x,y). If no focus yet, sets focus then sends. Used for two-finger tap / long-press. */
    external fun nativeOnPointerRightClick(x: Float, y: Float, timeMs: Int)

    /** Set cursor draw position in surface pixels (call before/with pointer events so cursor follows touch). */
    external fun nativeSetCursorPhysical(x: Float, y: Float)

    /** Show cursor in touchpad mode, hide in tablet mode (touch = finger, no cursor like phone). */
    external fun nativeSetCursorVisible(visible: Boolean)

    /**
     * Stop the Wayland server and tear down resources.
     *
     * Note: if the UI is currently rendering, callers should destroy the Surface first.
     */
    external fun nativeStopWayland()

    /** True if EGL/render is ready. */
    external fun nativeIsWaylandReady(): Boolean

    /** Current output size [width, height]. */
    external fun nativeGetOutputSize(): IntArray?

    /** Suggested XDG_RUNTIME_DIR for clients (e.g. filesDir/usr/tmp). */
    external fun nativeGetSocketDir(filesDir: String): String?

    /** True if any xdg_toplevel client (e.g. desktop) is connected; use to avoid re-running Display startup script. */
    external fun nativeHasActiveClients(): Boolean

    /**
     * Deliver a key event to the focused Wayland client (wl_keyboard). Used for approach A: Android as keyboard.
     * @param keyCode Android KeyEvent.KEYCODE_*
     * @param metaState KeyEvent.metaState
     * @param isDown true for ACTION_DOWN, false for ACTION_UP
     * @param timeMs event time in milliseconds (e.g. KeyEvent.eventTime)
     */
    external fun nativeOnKeyEvent(keyCode: Int, metaState: Int, isDown: Boolean, timeMs: Long)

    // nativeCommitTextUtf8 removed: we now inject Unicode via keyboard events (Ctrl+Shift+U).
}
