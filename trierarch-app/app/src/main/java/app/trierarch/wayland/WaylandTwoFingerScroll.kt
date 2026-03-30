package app.trierarch.wayland

import android.view.MotionEvent
import app.trierarch.WaylandBridge
import app.trierarch.ui.screens.MOUSE_MODE_TOUCHPAD

/**
 * Handles two-finger scroll and two-finger tap -> right click in touchpad mode.
 *
 * Contract:
 * - Call [onTouchEvent] for all non-mouse MotionEvents.
 * - Returns true when the event is consumed by scroll/two-finger-tap tracking.
 */
internal class WaylandTwoFingerScroll(
    private val coordMapper: WaylandCoordMapper
) {
    private var scrollLastCentroidX: Float? = null
    private var scrollLastCentroidY: Float? = null
    private var scrollJustEnded = false
    private var twoFingerStartCx = 0f
    private var twoFingerStartCy = 0f
    private var twoFingerStartTime = 0L
    private var twoFingerMaxDistSq = 0f
    private var twoFingerTapPending = false

    private companion object {
        const val TWO_FINGER_TAP_MAX_DURATION_MS = 400L
        const val TWO_FINGER_TAP_THRESHOLD = 22f
    }

    fun markNewGesture() {
        scrollJustEnded = false
        twoFingerTapPending = false
    }

    fun didScrollJustEndInTabletMode(mouseMode: Int, pointerCount: Int): Boolean {
        return scrollJustEnded && mouseMode != MOUSE_MODE_TOUCHPAD && pointerCount == 1
    }

    fun onTouchEvent(event: MotionEvent, mouseMode: Int, timeMs: Int): Boolean {
        if (event.pointerCount >= 2) {
            val cx = (0 until event.pointerCount).map { event.getX(it) }.average().toFloat()
            val cy = (0 until event.pointerCount).map { event.getY(it) }.average().toFloat()
            if (scrollLastCentroidX == null) {
                twoFingerStartCx = cx
                twoFingerStartCy = cy
                twoFingerStartTime = event.eventTime
                twoFingerMaxDistSq = 0f
            }
            if (event.actionMasked == MotionEvent.ACTION_MOVE) {
                twoFingerMaxDistSq = maxOf(twoFingerMaxDistSq, (cx - twoFingerStartCx).let { dx ->
                    (cy - twoFingerStartCy).let { dy -> dx * dx + dy * dy }
                })
                val lastCx = scrollLastCentroidX
                val lastCy = scrollLastCentroidY
                if (lastCx != null && lastCy != null) {
                    WaylandBridge.nativeOnPointerAxis(cx - lastCx, cy - lastCy, timeMs, WaylandBridge.AXIS_SOURCE_FINGER)
                }
            }
            scrollLastCentroidX = cx
            scrollLastCentroidY = cy
            return true
        }

        if (scrollLastCentroidX != null && scrollLastCentroidY != null) {
            if (mouseMode == MOUSE_MODE_TOUCHPAD && event.pointerCount == 1 &&
                event.eventTime - twoFingerStartTime <= TWO_FINGER_TAP_MAX_DURATION_MS &&
                twoFingerMaxDistSq < TWO_FINGER_TAP_THRESHOLD * TWO_FINGER_TAP_THRESHOLD
            ) {
                twoFingerTapPending = true
                coordMapper.setCursorPhysical(coordMapper.cursorX, coordMapper.cursorY)
                val w = coordMapper.toWaylandCoords(coordMapper.cursorX, coordMapper.cursorY)
                WaylandBridge.nativeOnPointerRightClick(w[0], w[1], timeMs)
            }
            scrollJustEnded = true
            scrollLastCentroidX = null
            scrollLastCentroidY = null
            return true
        }

        if (twoFingerTapPending && mouseMode == MOUSE_MODE_TOUCHPAD &&
            (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_POINTER_UP)
        ) {
            twoFingerTapPending = false
            return true
        }

        scrollLastCentroidX = null
        scrollLastCentroidY = null
        return false
    }
}

