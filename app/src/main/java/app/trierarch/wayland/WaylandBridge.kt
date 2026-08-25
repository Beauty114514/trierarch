package app.trierarch.wayland

import android.content.Context
import android.view.Surface
import java.io.File

object WaylandBridge {
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

    private external fun nativeStart(runtimeDirectory: String): Boolean
    private external fun nativeStop()
    private external fun nativeAttachSurface(surface: Surface)
    private external fun nativeSetOutputSize(width: Int, height: Int)
}
