package app.trierarch.wayland

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.MotionEvent
import android.graphics.PixelFormat
import app.trierarch.input.PointerInputRouter

/** Full-screen, display-only target for the first Wayland milestone. */
class WaylandSurfaceView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {
    private val inputRouter = PointerInputRouter(context, WaylandPointerEventSink) {
        WaylandBridge.setCursorVisible(it)
    }

    init {
        holder.setFormat(PixelFormat.RGBA_8888)
        holder.addCallback(this)
        // The pixels are rendered by the separate Surface. Keeping the
        // ordinary View layer transparent prevents it from covering that
        // Surface with an opaque background, matching the X11 host view.
        setBackgroundColor(android.graphics.Color.TRANSPARENT)
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        WaylandBridge.attachSurface(holder.surface)
        updateOutputSize(holder.surfaceFrame.width(), holder.surfaceFrame.height())
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        updateOutputSize(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) = Unit

    override fun onTouchEvent(event: MotionEvent): Boolean = inputRouter.onTouchEvent(this, event)

    override fun onGenericMotionEvent(event: MotionEvent): Boolean =
        inputRouter.onGenericMotionEvent(this, event) || super.onGenericMotionEvent(event)

    override fun onHoverEvent(event: MotionEvent): Boolean =
        inputRouter.onHoverEvent(this, event) || super.onHoverEvent(event)

    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        if (!hasWindowFocus) inputRouter.cancel(this)
    }

    override fun onDetachedFromWindow() {
        inputRouter.cancel(this)
        super.onDetachedFromWindow()
    }

    private fun updateOutputSize(width: Int, height: Int) {
        WaylandBridge.setOutputSize(width, height)
    }
}
