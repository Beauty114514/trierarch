package app.trierarch.wayland

import android.os.Handler
import android.os.SystemClock
import android.view.MotionEvent
import app.trierarch.WaylandBridge

/**
 * Tablet-style absolute pointer controller for tablet mode.
 */
internal class WaylandTabletController(
    private val coordMapper: WaylandCoordMapper,
    private val handler: Handler
) {
    private var touchStartX = 0f
    private var touchStartY = 0f
    private var tabletLongPressRunnable: Runnable? = null
    private var tabletCommittedToDrag = false
    private var tabletLongPressFired = false
    private var tabletPendingWx = 0f
    private var tabletPendingWy = 0f

    private companion object {
        const val TABLET_DRAG_SLOP = 4f
        const val LONG_PRESS_RIGHT_MS = 500L
    }

    fun onTouchEvent(event: MotionEvent, timeMs: Int): Boolean {
        val idx = event.actionIndex
        val x = event.getX(idx)
        val y = event.getY(idx)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                touchStartX = x
                touchStartY = y
                tabletCommittedToDrag = false
                tabletLongPressFired = false
                coordMapper.setCursorPhysical(x, y)
                tabletLongPressRunnable?.let { handler.removeCallbacks(it) }
                val w = coordMapper.toWaylandCoords(touchStartX, touchStartY)
                tabletPendingWx = w[0]
                tabletPendingWy = w[1]
                val startX = touchStartX
                val startY = touchStartY
                val runnable = Runnable {
                    if (!tabletCommittedToDrag && !tabletLongPressFired) {
                        tabletLongPressFired = true
                        try {
                            val t = (SystemClock.uptimeMillis() and 0x7FFFFFFFL).toInt()
                            coordMapper.setCursorPhysical(startX, startY)
                            WaylandBridge.nativeOnPointerRightClick(tabletPendingWx, tabletPendingWy, t)
                        } catch (_: Throwable) {
                        }
                    }
                }
                tabletLongPressRunnable = runnable
                handler.postDelayed(runnable, LONG_PRESS_RIGHT_MS)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = x - touchStartX
                val dy = y - touchStartY
                val distSq = dx * dx + dy * dy
                coordMapper.setCursorPhysical(x, y)
                val w = coordMapper.toWaylandCoords(x, y)
                WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                if (!tabletCommittedToDrag && !tabletLongPressFired &&
                    distSq > TABLET_DRAG_SLOP * TABLET_DRAG_SLOP
                ) {
                    tabletLongPressRunnable?.let { handler.removeCallbacks(it) }
                    tabletLongPressRunnable = null
                    tabletCommittedToDrag = true
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_DOWN, timeMs)
                }
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                tabletLongPressRunnable?.let { handler.removeCallbacks(it) }
                tabletLongPressRunnable = null
                when {
                    tabletLongPressFired -> {}
                    tabletCommittedToDrag -> {
                        coordMapper.setCursorPhysical(x, y)
                        val upW = coordMapper.toWaylandCoords(x, y)
                        WaylandBridge.nativeOnPointerEvent(upW[0], upW[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                    }
                    else -> {
                        val startW = coordMapper.toWaylandCoords(touchStartX, touchStartY)
                        val upW = coordMapper.toWaylandCoords(x, y)
                        coordMapper.setCursorPhysical(touchStartX, touchStartY)
                        WaylandBridge.nativeOnPointerEvent(startW[0], startW[1], WaylandBridge.POINTER_ACTION_DOWN, timeMs)
                        coordMapper.setCursorPhysical(x, y)
                        WaylandBridge.nativeOnPointerEvent(upW[0], upW[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                    }
                }
                return true
            }
        }
        return false
    }

    fun cancel() {
        tabletLongPressRunnable?.let { handler.removeCallbacks(it) }
        tabletLongPressRunnable = null
    }
}

