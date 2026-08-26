package app.trierarch.wayland

import android.os.SystemClock
import app.trierarch.input.PointerEventSink

/** Forwards protocol-independent Android pointer events to the Wayland compositor. */
internal object WaylandPointerEventSink : PointerEventSink {
    override fun moveRelative(deltaX: Float, deltaY: Float) {
        WaylandBridge.movePointerRelative(deltaX, deltaY, now())
    }

    override fun moveAbsolute(x: Float, y: Float) {
        WaylandBridge.movePointerAbsolute(x, y, now())
    }

    override fun setButton(button: Int, pressed: Boolean) {
        WaylandBridge.setPointerButton(button, pressed, now())
    }

    override fun scroll(deltaX: Float, deltaY: Float, source: PointerEventSink.ScrollSource) {
        val axisSource = when (source) {
            PointerEventSink.ScrollSource.FINGER -> WaylandBridge.AXIS_SOURCE_FINGER
            PointerEventSink.ScrollSource.WHEEL -> WaylandBridge.AXIS_SOURCE_WHEEL
        }
        WaylandBridge.scrollPointer(deltaX, deltaY, axisSource, now())
    }

    override fun cancel() {
        WaylandBridge.resetPointer(now())
    }

    private fun now(): Int = (SystemClock.uptimeMillis() and 0x7fff_ffffL).toInt()
}
