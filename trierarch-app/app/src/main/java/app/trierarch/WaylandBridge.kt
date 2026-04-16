package app.trierarch

import android.view.Surface

/** JNI to `libwayland-compositor.so`. Start proot before [nativeStartServer]. */
object WaylandBridge {

    const val POINTER_ACTION_DOWN = 0
    const val POINTER_ACTION_MOVE = 1
    const val POINTER_ACTION_UP = 2
    const val POINTER_ACTION_POINTER_MOVE = 6

    const val AXIS_SOURCE_FINGER = 1

    init {
        try {
            System.loadLibrary("wayland-compositor")
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    external fun nativeStartServer(runtimeDir: String)

    /** @param skipEglWaylandBind keep false unless all buffers are linux-dmabuf import path. */
    external fun nativeSurfaceCreated(
        surface: Surface,
        runtimeDir: String,
        resolutionPercent: Int,
        scalePercent: Int,
        skipEglWaylandBind: Boolean,
    )

    external fun nativeSurfaceDestroyed()

    external fun nativeOutputSizeChanged(width: Int, height: Int, resolutionPercent: Int, scalePercent: Int)

    external fun nativeOnPointerEvent(x: Float, y: Float, action: Int, timeMs: Int)

    external fun nativeOnPointerAxis(deltaX: Float, deltaY: Float, timeMs: Int, axisSource: Int)

    external fun nativeOnPointerRightClick(x: Float, y: Float, timeMs: Int)

    external fun nativeSetCursorPhysical(x: Float, y: Float)

    external fun nativeSetCursorVisible(visible: Boolean)

    external fun nativeGetOutputSize(): IntArray?

    external fun nativeHasActiveClients(): Boolean

    external fun nativeOnKeyEvent(keyCode: Int, metaState: Int, isDown: Boolean, timeMs: Long)

    /** Clear stuck modifiers (Shift/Ctrl/Alt/Meta/Caps/Num/Scroll) in the in-process compositor. */
    external fun nativeResetKeyboardState()
}
