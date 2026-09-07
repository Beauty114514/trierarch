package app.trierarch.wayland

import android.content.Context
import android.view.Surface
import java.io.File

object WaylandBridge {
    const val AXIS_SOURCE_WHEEL = 0
    const val AXIS_SOURCE_FINGER = 1
    init {
        System.loadLibrary("wayland-server")
        System.loadLibrary("ffi")
        System.loadLibrary("trierarch-wayland-host")
    }

    @JvmStatic
    fun start(context: Context): Boolean {
        val runtime = File(context.filesDir, "wayland/runtime")
        check(runtime.isDirectory || runtime.mkdirs()) { "Unable to create Wayland runtime directory" }
        return nativeStart(runtime.absolutePath)
    }

    @JvmStatic
    fun stop() = nativeStop()

    @JvmStatic
    fun attachSurface(surface: Surface) = nativeAttachSurface(surface)

    @JvmStatic
    fun setOutputSize(width: Int, height: Int) {
        if (width > 0 && height > 0) nativeSetOutputSize(width, height)
    }

    @JvmStatic fun movePointerAbsolute(x: Float, y: Float, timeMillis: Int) =
        nativeMovePointerAbsolute(x, y, timeMillis)

    @JvmStatic fun movePointerRelative(deltaX: Float, deltaY: Float, timeMillis: Int) =
        nativeMovePointerRelative(deltaX, deltaY, timeMillis)

    @JvmStatic fun setPointerButton(button: Int, pressed: Boolean, timeMillis: Int) =
        nativeSetPointerButton(button, pressed, timeMillis)

    @JvmStatic fun scrollPointer(deltaX: Float, deltaY: Float, source: Int, timeMillis: Int) =
        nativeScrollPointer(deltaX, deltaY, source, timeMillis)

    @JvmStatic fun resetPointer(timeMillis: Int) = nativeResetPointer(timeMillis)

    @JvmStatic fun setKeyboardKey(
        keyCode: Int,
        scanCode: Int,
        pressed: Boolean,
        timeMillis: Int,
    ): Boolean = nativeSetKeyboardKey(keyCode, scanCode, pressed, timeMillis)

    @JvmStatic fun setCursorVisible(visible: Boolean) = nativeSetCursorVisible(visible)

    private external fun nativeStart(runtimeDirectory: String): Boolean
    private external fun nativeStop()
    private external fun nativeAttachSurface(surface: Surface)
    private external fun nativeSetOutputSize(width: Int, height: Int)
    private external fun nativeMovePointerAbsolute(x: Float, y: Float, timeMillis: Int)
    private external fun nativeMovePointerRelative(deltaX: Float, deltaY: Float, timeMillis: Int)
    private external fun nativeSetPointerButton(button: Int, pressed: Boolean, timeMillis: Int)
    private external fun nativeScrollPointer(deltaX: Float, deltaY: Float, source: Int, timeMillis: Int)
    private external fun nativeResetPointer(timeMillis: Int)
    private external fun nativeSetKeyboardKey(
        keyCode: Int,
        scanCode: Int,
        pressed: Boolean,
        timeMillis: Int,
    ): Boolean
    private external fun nativeSetCursorVisible(visible: Boolean)
}
