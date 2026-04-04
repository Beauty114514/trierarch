package app.trierarch.wayland

import android.os.Handler
import android.os.SystemClock
import android.view.MotionEvent
import app.trierarch.WaylandBridge

/**
 * Simulated touchpad controller (relative cursor) for touchpad mode.
 */
internal class WaylandTouchpadController(
    private val coordMapper: WaylandCoordMapper,
    private val handler: Handler
) {
    private var lastX = 0f
    private var lastY = 0f
    private var touchStartX = 0f
    private var touchStartY = 0f
    private var touchStartTime = 0L
    private var touchpadButtonDown = false
    private var touchpadAwaitingHoldDrag = false
    private var holdDragRunnable: Runnable? = null

    private companion object {
        const val TAP_THRESHOLD = 15f
        const val TOUCHPAD_TAP_MAX_MS = 180L
        const val TOUCHPAD_HOLD_DRAG_MS = 220L
    }

    fun onTouchEvent(event: MotionEvent, timeMs: Int): Boolean {
        val idx = event.actionIndex
        val x = event.getX(idx)
        val y = event.getY(idx)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = x
                lastY = y
                touchStartX = x
                touchStartY = y
                touchStartTime = event.eventTime
                touchpadButtonDown = false
                touchpadAwaitingHoldDrag = true
                holdDragRunnable?.let { handler.removeCallbacks(it) }
                val holdDrag = Runnable {
                    holdDragRunnable = null
                    if (!touchpadAwaitingHoldDrag || touchpadButtonDown) return@Runnable
                    val tdx = lastX - touchStartX
                    val tdy = lastY - touchStartY
                    if (tdx * tdx + tdy * tdy > TAP_THRESHOLD * TAP_THRESHOLD) return@Runnable
                    touchpadAwaitingHoldDrag = false
                    touchpadButtonDown = true
                    coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
                    val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_DOWN, coordMapper.nowTimeMs32())
                }
                holdDragRunnable = holdDrag
                handler.postDelayed(holdDrag, TOUCHPAD_HOLD_DRAG_MS)
                return true
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                holdDragRunnable?.let { handler.removeCallbacks(it) }
                holdDragRunnable = null
                touchpadAwaitingHoldDrag = false
                if (touchpadButtonDown) {
                    coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
                    val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                    touchpadButtonDown = false
                }
                lastX = x
                lastY = y
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = x - lastX
                val dy = y - lastY
                lastX = x
                lastY = y
                val totalDx = x - touchStartX
                val totalDy = y - touchStartY
                val distSq = totalDx * totalDx + totalDy * totalDy
                if (touchpadAwaitingHoldDrag && !touchpadButtonDown && distSq > TAP_THRESHOLD * TAP_THRESHOLD) {
                    holdDragRunnable?.let { handler.removeCallbacks(it) }
                    holdDragRunnable = null
                    touchpadAwaitingHoldDrag = false
                }
                coordMapper.moveCursorBy(dx, dy)
                val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
                coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
                WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                return true
            }
            MotionEvent.ACTION_UP -> {
                holdDragRunnable?.let { handler.removeCallbacks(it) }
                holdDragRunnable = null
                touchpadAwaitingHoldDrag = false
                coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
                val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
                if (touchpadButtonDown) {
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                } else {
                    val totalDx = x - touchStartX
                    val totalDy = y - touchStartY
                    val distSq = totalDx * totalDx + totalDy * totalDy
                    val duration = event.eventTime - touchStartTime
                    if (distSq <= TAP_THRESHOLD * TAP_THRESHOLD && duration <= TOUCHPAD_TAP_MAX_MS) {
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_DOWN, timeMs)
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                    }
                }
                touchpadButtonDown = false
                return true
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val stayIdx = 1 - event.actionIndex
                if (event.pointerCount >= 2 && stayIdx in 0 until event.pointerCount) {
                    lastX = event.getX(stayIdx)
                    lastY = event.getY(stayIdx)
                }
                return true
            }
        }
        return false
    }

    fun cancel() {
        holdDragRunnable?.let { handler.removeCallbacks(it) }
        holdDragRunnable = null
    }

    /**
     * Called when a second (or more) finger lands without [onTouchEvent] receiving [ACTION_POINTER_DOWN]
     * (multi-touch is handled elsewhere first). Cancels the delayed hold-drag runnable and mirrors
     * the state cleanup in [ACTION_POINTER_DOWN].
     */
    fun onMultiTouchGestureStarted(event: MotionEvent, timeMs: Int) {
        holdDragRunnable?.let { handler.removeCallbacks(it) }
        holdDragRunnable = null
        touchpadAwaitingHoldDrag = false
        if (touchpadButtonDown) {
            coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
            val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
            WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
            touchpadButtonDown = false
        }
        lastX = event.getX(0)
        lastY = event.getY(0)
    }

    /**
     * [WaylandTwoFingerScroll] consumes the final [ACTION_UP] after a two-finger tap → right click.
     * Sync touchpad bookkeeping so the next single-finger stream does not see stale state.
     */
    fun onTwoFingerTapUpConsumed(event: MotionEvent, timeMs: Int) {
        holdDragRunnable?.let { handler.removeCallbacks(it) }
        holdDragRunnable = null
        touchpadAwaitingHoldDrag = false
        if (touchpadButtonDown) {
            coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
            val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
            WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
            touchpadButtonDown = false
        }
        if (event.pointerCount > 0) {
            val idx = event.actionIndex.coerceIn(0, event.pointerCount - 1)
            lastX = event.getX(idx)
            lastY = event.getY(idx)
        }
    }
}

