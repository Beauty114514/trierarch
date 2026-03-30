package app.trierarch.wayland

import android.os.SystemClock
import app.trierarch.WaylandBridge

/**
 * Maps Android view coordinates into Trierarch Wayland logical output coordinates.
 *
 * Contract:
 * - Call [onViewSizeChanged] from the host view.
 * - Call [setSurfaceSize] when SurfaceView reports a size.
 * - Uses [WaylandBridge.nativeGetOutputSize] as the logical reference.
 */
internal class WaylandCoordMapper {
    private var viewWidth: Int = 0
    private var viewHeight: Int = 0
    private var surfaceWidth: Int = 0
    private var surfaceHeight: Int = 0

    var cursorX: Float = 0f
        private set
    var cursorY: Float = 0f
        private set

    fun onViewSizeChanged(w: Int, h: Int) {
        viewWidth = w
        viewHeight = h
        if (w > 0 && h > 0) {
            cursorX = (w / 2).toFloat()
            cursorY = (h / 2).toFloat()
        }
    }

    fun setSurfaceSize(w: Int, h: Int) {
        surfaceWidth = w
        surfaceHeight = h
    }

    fun setCursorPhysical(x: Float, y: Float) {
        cursorX = x
        cursorY = y
        WaylandBridge.nativeSetCursorPhysical(x, y)
    }

    fun moveCursorBy(dx: Float, dy: Float) {
        val refW = referenceWidth().coerceAtLeast(1)
        val refH = referenceHeight().coerceAtLeast(1)
        cursorX = (cursorX + dx).coerceIn(0f, (refW - 1).toFloat())
        cursorY = (cursorY + dy).coerceIn(0f, (refH - 1).toFloat())
    }

    fun toWaylandCoords(viewX: Float, viewY: Float): FloatArray {
        val out = WaylandBridge.nativeGetOutputSize() ?: return floatArrayOf(viewX, viewY)
        if (out.size < 2 || out[0] <= 0 || out[1] <= 0) return floatArrayOf(viewX, viewY)

        val refW = referenceWidth()
        val refH = referenceHeight()
        if (refW <= 0 || refH <= 0) return floatArrayOf(viewX, viewY)

        val lw = out[0].toFloat()
        val lh = out[1].toFloat()
        val x = viewX.coerceIn(0f, refW.toFloat())
        val y = viewY.coerceIn(0f, refH.toFloat())
        val wx = (x * lw / refW).coerceIn(0f, (lw - 0.5f).coerceAtLeast(0f))
        val wy = (y * lh / refH).coerceIn(0f, (lh - 0.5f).coerceAtLeast(0f))
        return floatArrayOf(wx, wy)
    }

    fun nowTimeMs32(): Int = (SystemClock.uptimeMillis() and 0x7FFFFFFFL).toInt()

    private fun referenceWidth(): Int = if (surfaceWidth > 0) surfaceWidth else viewWidth
    private fun referenceHeight(): Int = if (surfaceHeight > 0) surfaceHeight else viewHeight
}

