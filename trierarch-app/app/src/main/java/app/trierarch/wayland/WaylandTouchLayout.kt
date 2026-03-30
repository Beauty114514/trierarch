package app.trierarch.wayland

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import android.widget.FrameLayout
import androidx.lifecycle.LifecycleOwner
import app.trierarch.WaylandBridge
import app.trierarch.input.SoftKeyboardView
import app.trierarch.ui.screens.MOUSE_MODE_TABLET
import app.trierarch.ui.screens.MOUSE_MODE_TOUCHPAD

/**
 * Touch + mouse input router and glue for the embedded Wayland compositor.
 *
 * Responsibilities:
 * - Converts Android input events into Trierarch Wayland pointer/axis events.
 * - Maintains a simulated cursor for touchpad mode.
 * - Owns "cursor visibility policy" (avoid double cursors with a physical mouse).
 * - Owns "soft keyboard recovery policy" for the hidden IME sink view.
 *
 * Constraints:
 * - This view is embedded in Compose via [androidx.compose.ui.viewinterop.AndroidView].
 * - It must be safe to call from UI thread only.
 */
internal class WaylandTouchLayout(context: Context) : FrameLayout(context) {
    var mouseMode: Int = 0
    var resolutionPercent: Int = 100
    var scalePercent: Int = 100
    var lastSurfaceWidth: Int = 0
    var lastSurfaceHeight: Int = 0
    var lastAppliedResolutionPercent: Int = -1
    var lastAppliedScalePercent: Int = -1
    var keyboardSinkView: SoftKeyboardView? = null

    private val mainHandler = Handler(Looper.getMainLooper())
    private val imeRecovery = WaylandImeRecovery(context) { keyboardSinkView }
    private val cursorPolicy = WaylandCursorVisibilityPolicy(mainHandler)

    private val coordMapper = WaylandCoordMapper()
    private val touchpadController = WaylandTouchpadController(coordMapper, mainHandler)
    private val tabletController = WaylandTabletController(coordMapper, mainHandler)
    private val twoFingerScroll = WaylandTwoFingerScroll(coordMapper)

    fun setKeyboardWanted(wanted: Boolean) {
        imeRecovery.setWanted(wanted)
    }

    fun bindLifecycleOwner(owner: LifecycleOwner?) {
        imeRecovery.bindLifecycleOwner(owner)
    }

    /**
     * Soft keyboard recovery policy ("scheme 1"):
     *
     * Once the user explicitly requests the keyboard, we keep it "wanted" while the Wayland view
     * is on screen and re-issue `showSoftInput()` at key lifecycle boundaries:
     * - app resumes / window regains focus
     * - default IME changes (e.g. switching between keyboard apps)
     *
     * Rationale: some IMEs hide the current window during switching and the new IME does not
     * automatically re-show unless the app requests it again.
     */
    fun ensureSoftKeyboardVisible() {
        imeRecovery.ensureVisible()
    }

    /**
     * Cursor visibility policy:
     *
     * - When a physical mouse is active, Android draws a system cursor that apps cannot reliably hide.
     *   In that case we hide Trierarch's Wayland cursor to avoid the "double cursor" effect.
     * - When the user drives the cursor via touchpad simulation, we show the Wayland cursor.
     */
    fun applyCursorVisibilityPolicy() {
        cursorPolicy.apply(mouseMode)
    }

    fun onSurfaceSizeChanged(w: Int, h: Int) {
        coordMapper.setSurfaceSize(w, h)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        imeRecovery.onAttachedToWindow()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        imeRecovery.onDetachedFromWindow()
        cursorPolicy.cancel()
        touchpadController.cancel()
        tabletController.cancel()
    }

    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        imeRecovery.onWindowFocusChanged(hasWindowFocus)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        coordMapper.onViewSizeChanged(w, h)
    }

    override fun onInterceptTouchEvent(ev: MotionEvent): Boolean = true

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val timeMs = (event.eventTime and 0x7FFFFFFFL).toInt()
        val idx = event.actionIndex
        val x = event.getX(idx)
        val y = event.getY(idx)

        if ((event.source and InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) {
            cursorPolicy.notePhysicalMouseActivity()
            applyCursorVisibilityPolicy()
            val w = coordMapper.toWaylandCoords(x, y)
            coordMapper.setCursorPhysical(x, y)
            when (event.actionMasked) {
                MotionEvent.ACTION_MOVE, MotionEvent.ACTION_HOVER_MOVE -> {
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                }
                MotionEvent.ACTION_DOWN -> {
                    if ((event.buttonState and MotionEvent.BUTTON_SECONDARY) != 0) {
                        WaylandBridge.nativeOnPointerRightClick(w[0], w[1], timeMs)
                    } else {
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_DOWN, timeMs)
                    }
                }
                MotionEvent.ACTION_UP -> {
                    if ((event.buttonState and MotionEvent.BUTTON_SECONDARY) == 0) {
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                    }
                }
            }
            return true
        }

        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            twoFingerScroll.markNewGesture()
        }

        if (twoFingerScroll.onTouchEvent(event, mouseMode, timeMs)) {
            if (event.pointerCount == 1) {
                // Keep mapper surface size up to date for cursor movement clamping.
                coordMapper.setSurfaceSize(lastSurfaceWidth, lastSurfaceHeight)
            }
            return true
        }
        if (twoFingerScroll.didScrollJustEndInTabletMode(mouseMode, event.pointerCount)) return true

        val isTouchpad = mouseMode == MOUSE_MODE_TOUCHPAD
        if (isTouchpad) {
            cursorPolicy.noteTouchDrivenCursor()
            applyCursorVisibilityPolicy()
            coordMapper.setSurfaceSize(lastSurfaceWidth, lastSurfaceHeight)
            return touchpadController.onTouchEvent(event, timeMs)
        } else {
            // Tablet mode does not rely on the simulated touchpad cursor.
            return tabletController.onTouchEvent(event, timeMs)
        }
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        val timeMs = (event.eventTime and 0x7FFFFFFFL).toInt()
        if ((event.source and InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) {
            cursorPolicy.notePhysicalMouseActivity()
            applyCursorVisibilityPolicy()
            when (event.actionMasked) {
                MotionEvent.ACTION_SCROLL -> {
                    val v = event.getAxisValue(MotionEvent.AXIS_VSCROLL)
                    val h = event.getAxisValue(MotionEvent.AXIS_HSCROLL)
                    if (v != 0f || h != 0f) {
                        WaylandBridge.nativeOnPointerAxis(-h, -v, timeMs, axisSource = 0)
                    }
                    return true
                }
                MotionEvent.ACTION_HOVER_MOVE -> {
                    val x = event.x
                    val y = event.y
                    val w = coordMapper.toWaylandCoords(x, y)
                    coordMapper.setCursorPhysical(x, y)
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                    return true
                }
            }
        }
        return super.onGenericMotionEvent(event)
    }
}

