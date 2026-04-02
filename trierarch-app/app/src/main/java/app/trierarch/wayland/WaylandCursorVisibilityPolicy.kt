package app.trierarch.wayland

import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import app.trierarch.WaylandBridge
import app.trierarch.ui.dialog.MOUSE_MODE_TOUCHPAD

/**
 * Avoids the "double cursor" effect when a physical mouse is connected.
 *
 * Rationale:
 * - Android renders a system cursor for physical mice that apps cannot reliably hide.
 * - Trierarch also renders a Wayland cursor to support simulated touchpad input.
 * - When the physical mouse is active, we hide the Wayland cursor so only the system cursor remains.
 */
internal class WaylandCursorVisibilityPolicy(
    private val mainHandler: Handler = Handler(Looper.getMainLooper())
) {
    private var physicalMouseActiveUntilMs: Long = 0L
    private var probe: Runnable? = null

    private companion object {
        const val PHYSICAL_MOUSE_HIDE_CURSOR_MS = 4000L
    }

    fun notePhysicalMouseActivity() {
        physicalMouseActiveUntilMs = SystemClock.uptimeMillis() + PHYSICAL_MOUSE_HIDE_CURSOR_MS
        probe?.let { mainHandler.removeCallbacks(it) }
        val r = Runnable {
            probe = null
            // Re-apply after the timeout to restore Wayland cursor if needed.
            // The caller provides current mouseMode via apply(...).
        }
        probe = r
        mainHandler.postDelayed(r, PHYSICAL_MOUSE_HIDE_CURSOR_MS + 30)
    }

    fun noteTouchDrivenCursor() {
        physicalMouseActiveUntilMs = 0L
    }

    fun apply(mouseMode: Int) {
        val now = SystemClock.uptimeMillis()
        val physicalMouseActive = now < physicalMouseActiveUntilMs
        val wantWaylandCursor = (mouseMode == MOUSE_MODE_TOUCHPAD) && !physicalMouseActive
        try {
            WaylandBridge.nativeSetCursorVisible(wantWaylandCursor)
        } catch (_: Throwable) {
        }
    }

    fun cancel() {
        probe?.let { mainHandler.removeCallbacks(it) }
        probe = null
    }
}

