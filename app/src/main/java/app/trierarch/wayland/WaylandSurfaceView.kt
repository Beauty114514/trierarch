package app.trierarch.wayland

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.graphics.PixelFormat

/** Full-screen, display-only target for the first Wayland milestone. */
class WaylandSurfaceView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {
    init {
        holder.setFormat(PixelFormat.RGBA_8888)
        holder.addCallback(this)
        // The pixels are rendered by the separate Surface. Keeping the
        // ordinary View layer transparent prevents it from covering that
        // Surface with an opaque background, matching the X11 host view.
        setBackgroundColor(android.graphics.Color.TRANSPARENT)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        WaylandBridge.attachSurface(holder.surface)
        updateOutputSize(holder.surfaceFrame.width(), holder.surfaceFrame.height())
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        updateOutputSize(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) = Unit

    private fun updateOutputSize(width: Int, height: Int) {
        WaylandBridge.setOutputSize(width, height)
    }
}
