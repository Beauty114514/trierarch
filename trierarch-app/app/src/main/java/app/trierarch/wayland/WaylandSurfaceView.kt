package app.trierarch.wayland

import android.content.Context
import android.database.ContentObserver
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.Settings
import android.view.Gravity
import android.view.InputDevice
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.inputmethod.InputMethodManager
import android.widget.FrameLayout
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import app.trierarch.WaylandBridge
import app.trierarch.input.SoftKeyboardView
import app.trierarch.ui.screens.MOUSE_MODE_TABLET
import app.trierarch.ui.screens.MOUSE_MODE_TOUCHPAD

@Composable
fun WaylandSurfaceView(
    runtimeDir: String,
    mouseMode: Int,
    resolutionPercent: Int,
    scalePercent: Int,
    showKeyboardTrigger: Int,
    onKeyboardTriggerConsumed: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    /*
     * WaylandSurfaceView hosts the native compositor output inside a SurfaceView.
     *
     * Contract:
     * - The native server must already be started via WaylandBridge.nativeStartServer(runtimeDir).
     * - When the Surface becomes available we call nativeSurfaceCreated(...) which switches native code into
     *   "render mode" (EGL + render loop). When destroyed we call nativeSurfaceDestroyed() to tear down EGL and
     *   resume the lightweight dispatch thread.
     *
     * Input model (high level):
     * - Touchpad mode: one-finger moves a cursor relatively; taps click; two-finger scroll; two-finger tap = right click.
     * - Tablet mode: touch is absolute; tap clicks; long-press triggers right click or drag depending on movement.
     *
     * Note: This file intentionally documents *behavioral contracts* and non-obvious thresholds, not every line
     * of gesture code.
     */
    val rp = resolutionPercent.coerceIn(10, 100)
    val sp = scalePercent.coerceIn(10, 100)
    val lifecycleOwner = LocalLifecycleOwner.current
    AndroidView(
        factory = { ctx ->
            val layout = WaylandTouchLayout(ctx)
            layout.resolutionPercent = rp
            layout.scalePercent = sp
            layout.bindLifecycleOwner(lifecycleOwner)
            val surfaceView = SurfaceView(ctx).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(h: SurfaceHolder) {
                        val surface = h.surface ?: return
                        val l = (this@apply.parent as? WaylandTouchLayout)
                        val r = l?.resolutionPercent?.coerceIn(10, 100) ?: 100
                        val s = l?.scalePercent?.coerceIn(10, 100) ?: 100
                        WaylandBridge.nativeSurfaceCreated(surface, runtimeDir, r, s)
                        WaylandBridge.nativeSetCursorVisible((l?.mouseMode ?: MOUSE_MODE_TOUCHPAD) == MOUSE_MODE_TOUCHPAD)
                    }

                    override fun surfaceChanged(h: SurfaceHolder, format: Int, width: Int, height: Int) {
                        if (width > 0 && height > 0) {
                            val l = (this@apply.parent as? WaylandTouchLayout)
                            if (l != null) {
                                l.lastSurfaceWidth = width
                                l.lastSurfaceHeight = height
                                val r = l.resolutionPercent.coerceIn(10, 100)
                                val s = l.scalePercent.coerceIn(10, 100)
                                WaylandBridge.nativeOutputSizeChanged(width, height, r, s)
                                l.lastAppliedResolutionPercent = r
                                l.lastAppliedScalePercent = s
                            }
                        }
                    }

                    override fun surfaceDestroyed(h: SurfaceHolder) {
                        WaylandBridge.nativeSurfaceDestroyed()
                    }
                })
            }
            val keyboardView = SoftKeyboardView(ctx)
            layout.addView(surfaceView, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT, Gravity.CENTER))
            layout.addView(keyboardView, FrameLayout.LayoutParams(1, 1).apply {
                gravity = Gravity.TOP or Gravity.END
                marginEnd = -1
            })
            keyboardView.post { keyboardView.requestFocus() }
            layout.keyboardSinkView = keyboardView
            layout
        },
        update = { view ->
            val layout = view as? WaylandTouchLayout
            layout?.bindLifecycleOwner(lifecycleOwner)
            layout?.mouseMode = mouseMode
            layout?.resolutionPercent = rp
            layout?.scalePercent = sp
            WaylandBridge.nativeSetCursorVisible(mouseMode == MOUSE_MODE_TOUCHPAD)
            if (layout != null && layout.lastSurfaceWidth > 0 && layout.lastSurfaceHeight > 0 &&
                (layout.lastAppliedResolutionPercent != rp || layout.lastAppliedScalePercent != sp)
            ) {
                WaylandBridge.nativeOutputSizeChanged(layout.lastSurfaceWidth, layout.lastSurfaceHeight, rp, sp)
                layout.lastAppliedResolutionPercent = rp
                layout.lastAppliedScalePercent = sp
            }
            if (showKeyboardTrigger > 0) {
                layout?.setKeyboardWanted(true)
                layout?.ensureSoftKeyboardVisible()
                onKeyboardTriggerConsumed()
            }
        },
        modifier = modifier
    )
}

private class WaylandTouchLayout(context: Context) : FrameLayout(context) {
    var mouseMode: Int = 0
    var resolutionPercent: Int = 100
    var scalePercent: Int = 100
    var lastSurfaceWidth: Int = 0
    var lastSurfaceHeight: Int = 0
    var lastAppliedResolutionPercent: Int = -1
    var lastAppliedScalePercent: Int = -1
    var keyboardSinkView: SoftKeyboardView? = null
    private var keyboardWanted: Boolean = false
    private var lastShowSoftInputMs: Long = 0L
    private val mainHandler = Handler(Looper.getMainLooper())
    private var imeObserver: ContentObserver? = null
    private var lifecycleObserver: DefaultLifecycleObserver? = null
    private var boundLifecycleOwner: LifecycleOwner? = null
    private var pendingImeReshow: Runnable? = null
    private var lastX = 0f
    private var lastY = 0f
    private var viewWidth = 0
    private var viewHeight = 0
    private var cursorX = 0f
    private var cursorY = 0f
    private var touchStartX = 0f
    private var touchStartY = 0f
    private var touchStartTime = 0L
    private var scrollLastCentroidX: Float? = null
    private var scrollLastCentroidY: Float? = null
    private var scrollJustEnded = false
    private var twoFingerStartCx = 0f
    private var twoFingerStartCy = 0f
    private var twoFingerStartTime = 0L
    private var twoFingerMaxDistSq = 0f
    private var twoFingerTapPending = false
    private var touchpadButtonDown = false
    private val touchpadHandler = Handler(Looper.getMainLooper())
    private var touchpadHoldDragRunnable: Runnable? = null
    private var touchpadAwaitingHoldDrag = false
    private val tabletHandler = Handler(Looper.getMainLooper())
    private var tabletLongPressRunnable: Runnable? = null
    private var tabletCommittedToDrag = false
    private var tabletLongPressFired = false
    private var tabletPendingWx = 0f
    private var tabletPendingWy = 0f

    private companion object {
        /*
         * Gesture thresholds (tuned for phones; values are in physical pixels / milliseconds).
         *
         * We intentionally keep these values local to the input implementation:
         * - They are part of the UX contract (avoid accidental drags / right-clicks).
         * - They may require device-specific tuning; if you change one, test both modes.
         */
        const val TAP_THRESHOLD = 15f              // max movement to still count as a tap
        const val TABLET_DRAG_SLOP = 4f            // small movement before committing to drag in tablet mode
        const val LONG_PRESS_RIGHT_MS = 500L       // tablet-mode long press -> right click
        const val TWO_FINGER_TAP_MAX_DURATION_MS = 400L
        const val TWO_FINGER_TAP_THRESHOLD = 22f   // centroid movement threshold for two-finger tap
        const val TOUCHPAD_TAP_MAX_MS = 180L       // touchpad-mode tap click max duration
        const val TOUCHPAD_HOLD_DRAG_MS = 220L     // touchpad-mode hold -> begin drag (left button down)
        const val SOFT_INPUT_RESHOW_DEBOUNCE_MS = 250L  // avoid IME show loops during focus/IME changes
    }

    /**
     * Soft keyboard recovery policy ("scheme 1"):
     *
     * Once the user explicitly requests the keyboard, we keep it "wanted" while the Wayland view
     * is on screen and re-issue `showSoftInput()` at key lifecycle boundaries:
     * - app resumes / window regains focus
     * - default IME changes (e.g. switching from one keyboard app to another)
     *
     * Rationale: some IMEs hide the current window during switching and the new IME does not
     * automatically re-show unless the app requests it again.
     */
    fun setKeyboardWanted(wanted: Boolean) {
        keyboardWanted = wanted
    }

    fun bindLifecycleOwner(owner: LifecycleOwner?) {
        if (owner == boundLifecycleOwner) return

        boundLifecycleOwner?.let { prev ->
            lifecycleObserver?.let { obs ->
                prev.lifecycle.removeObserver(obs)
            }
        }
        boundLifecycleOwner = owner

        if (lifecycleObserver == null) {
            lifecycleObserver = object : DefaultLifecycleObserver {
                override fun onResume(owner: LifecycleOwner) {
                    ensureSoftKeyboardVisible()
                }
            }
        }
        if (owner != null && lifecycleObserver != null) {
            owner.lifecycle.addObserver(lifecycleObserver!!)
        }
    }

    fun ensureSoftKeyboardVisible() {
        if (!keyboardWanted) return
        val kv = keyboardSinkView ?: return
        val now = SystemClock.uptimeMillis()
        if (now - lastShowSoftInputMs < SOFT_INPUT_RESHOW_DEBOUNCE_MS) return
        lastShowSoftInputMs = now
        pendingImeReshow?.let { mainHandler.removeCallbacks(it) }
        pendingImeReshow = null

        fun attempt(delayMs: Long) {
            val r = Runnable {
                pendingImeReshow = null
                try {
                    kv.requestFocus()
                    val imm = kv.context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
                    if (imm != null) {
                        /*
                         * Some IMEs change process/connection during switching. Re-starting the input
                         * connection improves reliability of a subsequent showSoftInput() call.
                         */
                        imm.restartInput(kv)
                        val shown = imm.showSoftInput(kv, InputMethodManager.SHOW_IMPLICIT)
                        if (!shown) {
                            // Retry shortly: the IME switch can temporarily detach the window/token.
                            attempt(180)
                        }
                    }
                } catch (_: Throwable) {
                }
            }
            pendingImeReshow = r
            if (delayMs <= 0) mainHandler.post(r) else mainHandler.postDelayed(r, delayMs)
        }

        // First try immediately, then allow a short retry window for IME switches.
        attempt(0)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        if (imeObserver == null) {
            imeObserver = object : ContentObserver(mainHandler) {
                override fun onChange(selfChange: Boolean) {
                    // Default IME changed (e.g. switching between keyboard apps). Let the switch settle.
                    pendingImeReshow?.let { mainHandler.removeCallbacks(it) }
                    pendingImeReshow = Runnable { ensureSoftKeyboardVisible() }
                    mainHandler.postDelayed(pendingImeReshow!!, 120)
                }
            }
            try {
                context.contentResolver.registerContentObserver(
                    Settings.Secure.getUriFor(Settings.Secure.DEFAULT_INPUT_METHOD),
                    false,
                    imeObserver!!
                )
            } catch (_: Throwable) {
            }
        }

        // Lifecycle binding is provided by Compose (LocalLifecycleOwner) via bindLifecycleOwner(...).
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        bindLifecycleOwner(null)

        imeObserver?.let { obs ->
            try {
                context.contentResolver.unregisterContentObserver(obs)
            } catch (_: Throwable) {
            }
        }
        imeObserver = null
    }

    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        if (hasWindowFocus) ensureSoftKeyboardVisible()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        viewWidth = w
        viewHeight = h
        if (w > 0 && h > 0) {
            cursorX = (w / 2).toFloat()
            cursorY = (h / 2).toFloat()
        }
    }

    private fun toWaylandCoords(viewX: Float, viewY: Float): FloatArray {
        val out = WaylandBridge.nativeGetOutputSize() ?: return floatArrayOf(viewX, viewY)
        if (out.size < 2 || out[0] <= 0 || out[1] <= 0) return floatArrayOf(viewX, viewY)
        val refW = if (lastSurfaceWidth > 0) lastSurfaceWidth else viewWidth
        val refH = if (lastSurfaceHeight > 0) lastSurfaceHeight else viewHeight
        if (refW <= 0 || refH <= 0) return floatArrayOf(viewX, viewY)
        val lw = out[0].toFloat()
        val lh = out[1].toFloat()
        val x = viewX.coerceIn(0f, refW.toFloat())
        val y = viewY.coerceIn(0f, refH.toFloat())
        val wx = (x * lw / refW).coerceIn(0f, (lw - 0.5f).coerceAtLeast(0f))
        val wy = (y * lh / refH).coerceIn(0f, (lh - 0.5f).coerceAtLeast(0f))
        return floatArrayOf(wx, wy)
    }

    override fun onInterceptTouchEvent(ev: MotionEvent): Boolean = true

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val timeMs = (event.eventTime and 0x7FFFFFFFL).toInt()
        val idx = event.actionIndex
        val x = event.getX(idx)
        val y = event.getY(idx)

        if ((event.source and InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) {
            val w = toWaylandCoords(x, y)
            WaylandBridge.nativeSetCursorPhysical(x, y)
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
            scrollJustEnded = false
            twoFingerTapPending = false
        }

        if (event.pointerCount >= 2) {
            val cx = (0 until event.pointerCount).map { event.getX(it) }.average().toFloat()
            val cy = (0 until event.pointerCount).map { event.getY(it) }.average().toFloat()
            if (scrollLastCentroidX == null) {
                twoFingerStartCx = cx
                twoFingerStartCy = cy
                twoFingerStartTime = event.eventTime
                twoFingerMaxDistSq = 0f
            }
            when (event.actionMasked) {
                MotionEvent.ACTION_MOVE -> {
                    twoFingerMaxDistSq = maxOf(twoFingerMaxDistSq, (cx - twoFingerStartCx).let { dx ->
                        (cy - twoFingerStartCy).let { dy -> dx * dx + dy * dy }
                    })
                    val lastCx = scrollLastCentroidX
                    val lastCy = scrollLastCentroidY
                    if (lastCx != null && lastCy != null) {
                        WaylandBridge.nativeOnPointerAxis(cx - lastCx, cy - lastCy, timeMs, WaylandBridge.AXIS_SOURCE_FINGER)
                    }
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
                WaylandBridge.nativeSetCursorPhysical(cursorX, cursorY)
                val w = toWaylandCoords(cursorX, cursorY)
                WaylandBridge.nativeOnPointerRightClick(w[0], w[1], timeMs)
            }
            scrollJustEnded = true
            if (event.pointerCount == 1) {
                lastX = event.getX(0)
                lastY = event.getY(0)
            }
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

        if (scrollJustEnded && mouseMode == MOUSE_MODE_TABLET && event.pointerCount == 1) return true

        val isTouchpad = mouseMode == MOUSE_MODE_TOUCHPAD
        if (isTouchpad) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    lastX = x
                    lastY = y
                    touchStartX = x
                    touchStartY = y
                    touchStartTime = event.eventTime
                    touchpadButtonDown = false
                    touchpadAwaitingHoldDrag = true
                    touchpadHoldDragRunnable?.let { touchpadHandler.removeCallbacks(it) }
                    val holdDrag = Runnable {
                        touchpadHoldDragRunnable = null
                        if (!touchpadAwaitingHoldDrag || touchpadButtonDown) return@Runnable
                        val tdx = lastX - touchStartX
                        val tdy = lastY - touchStartY
                        if (tdx * tdx + tdy * tdy > TAP_THRESHOLD * TAP_THRESHOLD) return@Runnable
                        touchpadAwaitingHoldDrag = false
                        touchpadButtonDown = true
                        WaylandBridge.nativeSetCursorPhysical(cursorX, cursorY)
                        val w = toWaylandCoords(cursorX, cursorY)
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_DOWN, (SystemClock.uptimeMillis() and 0x7FFFFFFFL).toInt())
                    }
                    touchpadHoldDragRunnable = holdDrag
                    touchpadHandler.postDelayed(holdDrag, TOUCHPAD_HOLD_DRAG_MS)
                }
                MotionEvent.ACTION_POINTER_DOWN -> {
                    touchpadHoldDragRunnable?.let { touchpadHandler.removeCallbacks(it) }
                    touchpadHoldDragRunnable = null
                    touchpadAwaitingHoldDrag = false
                    if (touchpadButtonDown) {
                        WaylandBridge.nativeSetCursorPhysical(cursorX, cursorY)
                        val w = toWaylandCoords(cursorX, cursorY)
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                        touchpadButtonDown = false
                    }
                    lastX = x
                    lastY = y
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
                        touchpadHoldDragRunnable?.let { touchpadHandler.removeCallbacks(it) }
                        touchpadHoldDragRunnable = null
                        touchpadAwaitingHoldDrag = false
                    }
                    val refW = (if (lastSurfaceWidth > 0) lastSurfaceWidth else viewWidth).coerceAtLeast(1)
                    val refH = (if (lastSurfaceHeight > 0) lastSurfaceHeight else viewHeight).coerceAtLeast(1)
                    cursorX = (cursorX + dx).coerceIn(0f, (refW - 1).toFloat())
                    cursorY = (cursorY + dy).coerceIn(0f, (refH - 1).toFloat())
                    val w = toWaylandCoords(cursorX, cursorY)
                    WaylandBridge.nativeSetCursorPhysical(cursorX, cursorY)
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                }
                MotionEvent.ACTION_UP -> {
                    touchpadHoldDragRunnable?.let { touchpadHandler.removeCallbacks(it) }
                    touchpadHoldDragRunnable = null
                    touchpadAwaitingHoldDrag = false
                    WaylandBridge.nativeSetCursorPhysical(cursorX, cursorY)
                    val w = toWaylandCoords(cursorX, cursorY)
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
                }
                MotionEvent.ACTION_POINTER_UP -> {
                    val stayIdx = 1 - event.actionIndex
                    if (event.pointerCount >= 2 && stayIdx in 0 until event.pointerCount) {
                        lastX = event.getX(stayIdx)
                        lastY = event.getY(stayIdx)
                    }
                }
            }
        } else {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    touchStartX = x
                    touchStartY = y
                    touchStartTime = event.eventTime
                    tabletCommittedToDrag = false
                    tabletLongPressFired = false
                    WaylandBridge.nativeSetCursorPhysical(x, y)
                    tabletLongPressRunnable?.let { tabletHandler.removeCallbacks(it) }
                    val startX = touchStartX
                    val startY = touchStartY
                    val w = toWaylandCoords(startX, startY)
                    tabletPendingWx = w[0]
                    tabletPendingWy = w[1]
                    val runnable = Runnable {
                        if (!tabletCommittedToDrag && !tabletLongPressFired) {
                            tabletLongPressFired = true
                            try {
                                val t = (SystemClock.uptimeMillis() and 0x7FFFFFFFL).toInt()
                                WaylandBridge.nativeSetCursorPhysical(startX, startY)
                                WaylandBridge.nativeOnPointerRightClick(tabletPendingWx, tabletPendingWy, t)
                            } catch (_: Throwable) {
                            }
                        }
                    }
                    tabletLongPressRunnable = runnable
                    tabletHandler.postDelayed(runnable, LONG_PRESS_RIGHT_MS)
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = x - touchStartX
                    val dy = y - touchStartY
                    val distSq = dx * dx + dy * dy
                    WaylandBridge.nativeSetCursorPhysical(x, y)
                    val w = toWaylandCoords(x, y)
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                    if (!tabletCommittedToDrag && !tabletLongPressFired &&
                        distSq > TABLET_DRAG_SLOP * TABLET_DRAG_SLOP
                    ) {
                        tabletLongPressRunnable?.let { tabletHandler.removeCallbacks(it) }
                        tabletLongPressRunnable = null
                        tabletCommittedToDrag = true
                        WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_DOWN, timeMs)
                    }
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    tabletLongPressRunnable?.let { tabletHandler.removeCallbacks(it) }
                    tabletLongPressRunnable = null
                    when {
                        tabletLongPressFired -> {}
                        tabletCommittedToDrag -> {
                            WaylandBridge.nativeSetCursorPhysical(x, y)
                            val upW = toWaylandCoords(x, y)
                            WaylandBridge.nativeOnPointerEvent(upW[0], upW[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                        }
                        else -> {
                            val startW = toWaylandCoords(touchStartX, touchStartY)
                            val upW = toWaylandCoords(x, y)
                            WaylandBridge.nativeSetCursorPhysical(touchStartX, touchStartY)
                            WaylandBridge.nativeOnPointerEvent(startW[0], startW[1], WaylandBridge.POINTER_ACTION_DOWN, timeMs)
                            WaylandBridge.nativeSetCursorPhysical(x, y)
                            WaylandBridge.nativeOnPointerEvent(upW[0], upW[1], WaylandBridge.POINTER_ACTION_UP, timeMs)
                        }
                    }
                }
            }
        }
        return true
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        val timeMs = (event.eventTime and 0x7FFFFFFFL).toInt()
        if ((event.source and InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) {
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
                    val w = toWaylandCoords(x, y)
                    WaylandBridge.nativeSetCursorPhysical(x, y)
                    WaylandBridge.nativeOnPointerEvent(w[0], w[1], WaylandBridge.POINTER_ACTION_POINTER_MOVE, timeMs)
                    return true
                }
            }
        }
        return super.onGenericMotionEvent(event)
    }
}
