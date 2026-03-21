package app.trierarch

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.content.Context
import android.view.Gravity
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.inputmethod.InputMethodManager
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.AlertDialog
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import app.trierarch.input.WaylandKeyboardView
import app.trierarch.ui.screens.DisplayScriptDialog
import app.trierarch.ui.screens.InstallScreen
import app.trierarch.ui.screens.ViewSettingsDialog
import app.trierarch.ui.screens.SideMenu
import app.trierarch.ui.screens.TerminalScreen
import app.trierarch.ui.screens.MOUSE_MODE_TOUCHPAD
import app.trierarch.ui.screens.MOUSE_MODE_TABLET
import app.trierarch.ui.screens.twoFingerSwipeFromLeft
import app.trierarch.ui.AppStrings
import app.trierarch.ui.theme.TrierarchTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Enable or disable immersive fullscreen (like games): hide status bar and nav bar.
 * When enabled, user can swipe from edge to show them temporarily (BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE).
 * On API < 30 we only set flags once (no focus listener); bars may stay visible after a swipe until next interaction.
 */
private fun setImmersiveMode(activity: Activity?, immersive: Boolean) {
    val window = activity?.window ?: return
    val decorView = window.decorView
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        val controller = window.insetsController ?: return
        if (immersive) {
            controller.hide(WindowInsets.Type.systemBars())
            controller.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } else {
            controller.show(WindowInsets.Type.systemBars())
        }
    } else {
        @Suppress("DEPRECATION")
        decorView.systemUiVisibility = if (immersive) {
            (View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
        } else {
            0
        }
    }
}

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
    val rp = resolutionPercent.coerceIn(10, 100)
    val sp = scalePercent.coerceIn(10, 100)
    AndroidView(
        factory = { ctx ->
            val layout = WaylandTouchLayout(ctx)
            layout.resolutionPercent = rp
            layout.scalePercent = sp
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
            val keyboardView = WaylandKeyboardView(ctx)
            layout.addView(surfaceView, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT, Gravity.CENTER))
            layout.addView(keyboardView, FrameLayout.LayoutParams(1, 1).apply { gravity = Gravity.TOP or Gravity.END; marginEnd = -1 })
            layout.keyboardSinkView = keyboardView
            layout
        },
        update = { view ->
            val layout = view as? WaylandTouchLayout
            layout?.mouseMode = mouseMode
            layout?.resolutionPercent = rp
            layout?.scalePercent = sp
            WaylandBridge.nativeSetCursorVisible(mouseMode == MOUSE_MODE_TOUCHPAD)
            if (layout != null && layout.lastSurfaceWidth > 0 && layout.lastSurfaceHeight > 0 &&
                (layout.lastAppliedResolutionPercent != rp || layout.lastAppliedScalePercent != sp)) {
                WaylandBridge.nativeOutputSizeChanged(layout.lastSurfaceWidth, layout.lastSurfaceHeight, rp, sp)
                layout.lastAppliedResolutionPercent = rp
                layout.lastAppliedScalePercent = sp
            }
            if (showKeyboardTrigger > 0) {
                layout?.keyboardSinkView?.let { kv ->
                    kv.post {
                        kv.requestFocus()
                        val imm = kv.context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
                        imm?.showSoftInput(kv, InputMethodManager.SHOW_IMPLICIT)
                        onKeyboardTriggerConsumed()
                    }
                }
            }
        },
        modifier = modifier
    )
}

/** Forwards touch to Wayland pointer; uses [mouseMode] (MOUSE_MODE_TOUCHPAD / MOUSE_MODE_TABLET). */
private class WaylandTouchLayout(context: android.content.Context) : FrameLayout(context) {
    var mouseMode: Int = 0
    var resolutionPercent: Int = 100
    var scalePercent: Int = 100
    var lastSurfaceWidth: Int = 0
    var lastSurfaceHeight: Int = 0
    var lastAppliedResolutionPercent: Int = -1
    var lastAppliedScalePercent: Int = -1
    var keyboardSinkView: WaylandKeyboardView? = null
    private var lastX = 0f
    private var lastY = 0f

    /* Touchpad: relative movement; cursor position accumulated here */
    private var viewWidth = 0
    private var viewHeight = 0
    private var cursorX = 0f
    private var cursorY = 0f
    private var touchStartX = 0f
    private var touchStartY = 0f
    private var touchStartTime = 0L
    private var isMoving = false
    /* Two-finger scroll: centroid of fingers for axis delta */
    private var scrollLastCentroidX: Float? = null
    private var scrollLastCentroidY: Float? = null
    /* After 2→1 swallow: ignore single-finger position in tablet mode until all fingers lift */
    private var scrollJustEnded = false
    /* Two-finger tap → right-click: track start and movement to distinguish from scroll */
    private var twoFingerStartCx = 0f
    private var twoFingerStartCy = 0f
    private var twoFingerStartTime = 0L
    private var twoFingerMaxDistSq = 0f
    private var twoFingerTapPending = false
    /* Touchpad: tap = click; move = cursor; drag = hold still ~TOUCHPAD_HOLD_DRAG_MS then move (no "tap arms next touch" — avoids click-then-move → false drag). */
    private var touchpadButtonDown = false
    private val touchpadHandler = Handler(Looper.getMainLooper())
    private var touchpadHoldDragRunnable: Runnable? = null
    private var touchpadAwaitingHoldDrag = false
    /* Tablet mode: long-press = right-click; wait to distinguish tap / drag / long-press */
    private val tabletHandler = Handler(Looper.getMainLooper())
    private var tabletLongPressRunnable: Runnable? = null
    private var tabletCommittedToDrag = false
    private var tabletLongPressFired = false
    private var tabletPendingWx = 0f
    private var tabletPendingWy = 0f
    private companion object {
        /** Touchpad: slop (px) before treating as drag; moving cursor only must not trigger click. */
        const val TAP_THRESHOLD = 15f
        /** Tablet: slop (px) before drag; small so touch-and-drag starts quickly. */
        const val TABLET_DRAG_SLOP = 4f
        const val TAP_MAX_DURATION_MS = 250L
        const val LONG_PRESS_RIGHT_MS = 500L
        const val TWO_FINGER_TAP_MAX_DURATION_MS = 400L
        const val TWO_FINGER_TAP_THRESHOLD = 22f
        /** Touchpad: tap = down→up within this ms (libinput ~180ms). */
        const val TOUCHPAD_TAP_MAX_MS = 180L
        /** Touchpad: hold finger ~still this long, then button down for drag (same stroke; moving past slop before this cancels hold). */
        const val TOUCHPAD_HOLD_DRAG_MS = 220L
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

    /** Map touch coords to Wayland logical output coords. Use surface size (not layout) so pointer matches drawn content; clamp so (out_w, out_h) is never sent (client space is [0,w) x [0,h)). */
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

        /* Clear "scroll just ended" only on new touch, so we ignore all single-finger events until then */
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            scrollJustEnded = false
            twoFingerTapPending = false
        }

        /*
         * Touchpad: quick tap = click; move without hold = cursor only; drag = hold ~still (TOUCHPAD_HOLD_DRAG_MS) then move.
         * We do NOT arm "next touch after tap" for drag — that caused false drags after clicking dialogs (OK then move cursor).
         */
        /* Two-finger scroll or two-finger tap: send axis on move, or right-click if tap */
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
                    twoFingerMaxDistSq = maxOf(twoFingerMaxDistSq, (cx - twoFingerStartCx).let { dx -> (cy - twoFingerStartCy).let { dy -> dx * dx + dy * dy } })
                    val lastX = scrollLastCentroidX
                    val lastY = scrollLastCentroidY
                    if (lastX != null && lastY != null) {
                        WaylandBridge.nativeOnPointerAxis(cx - lastX, cy - lastY, timeMs, WaylandBridge.AXIS_SOURCE_FINGER)
                    }
                }
                else -> { }
            }
            scrollLastCentroidX = cx
            scrollLastCentroidY = cy
            return true
        }
        /* After two-finger: one finger just lifted. In touchpad mode: small/short → right-click now. */
        if (scrollLastCentroidX != null && scrollLastCentroidY != null) {
            if (mouseMode == MOUSE_MODE_TOUCHPAD && event.pointerCount == 1 &&
                event.eventTime - twoFingerStartTime <= TWO_FINGER_TAP_MAX_DURATION_MS &&
                twoFingerMaxDistSq < TWO_FINGER_TAP_THRESHOLD * TWO_FINGER_TAP_THRESHOLD) {
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
        /* Touchpad only: consume UP after two-finger tap (already sent right-click above) */
        if (twoFingerTapPending && mouseMode == MOUSE_MODE_TOUCHPAD &&
            (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_POINTER_UP)) {
            twoFingerTapPending = false
            return true
        }
        scrollLastCentroidX = null
        scrollLastCentroidY = null

        /* Tablet: until user lifts all fingers, ignore single-finger position after scroll */
        if (scrollJustEnded && mouseMode == MOUSE_MODE_TABLET && event.pointerCount == 1)
            return true

        val isTouchpad = (mouseMode == MOUSE_MODE_TOUCHPAD)
        if (isTouchpad) {
            /* Touchpad: tap = click; move = cursor; hold still then move = drag (see TOUCHPAD_HOLD_DRAG_MS). */
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    lastX = x
                    lastY = y
                    touchStartX = x
                    touchStartY = y
                    touchStartTime = event.eventTime
                    isMoving = false
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
                MotionEvent.ACTION_POINTER_UP -> { /* 2→1: keep lastX/lastY for the finger that stayed */
                    val idx = 1 - event.actionIndex
                    if (event.pointerCount >= 2 && idx in 0 until event.pointerCount) {
                        lastX = event.getX(idx)
                        lastY = event.getY(idx)
                    }
                }
            }
        } else {
            /* Tablet mode: touch = cursor position; long-press = right-click; move past slop = left drag */
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    touchStartX = x
                    touchStartY = y
                    touchStartTime = event.eventTime
                    isMoving = false
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
                            } catch (_: Throwable) { /* native not ready or server gone */ }
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
                        distSq > TABLET_DRAG_SLOP * TABLET_DRAG_SLOP) {
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
                        tabletLongPressFired -> { /* already sent right-click in runnable */ }
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
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            TrierarchTheme {
                TrierarchApp()
            }
        }
    }
}

@Composable
fun TrierarchApp() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    // App state
    var initialized by remember { mutableStateOf(false) }
    var hasRootfs by remember { mutableStateOf(false) }
    var installProgress by remember { mutableStateOf(0 to "Preparing...") }
    var installDone by remember { mutableStateOf(false) }
    var lines by remember { mutableStateOf(listOf<String>()) }
    var partialLine by remember { mutableStateOf("") }
    var inputLine by remember { mutableStateOf("") }
    var prootSpawned by remember { mutableStateOf(false) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var downloadStarted by remember { mutableStateOf(false) }
    var menuOpen by remember { mutableStateOf(false) }
    var waylandOn by remember { mutableStateOf(false) }   // Wayland server on/off (menu toggle)
    var showWayland by remember { mutableStateOf(false) } // Display: terminal vs Wayland view
    var settingsOpen by remember { mutableStateOf(false) }
    var displayScriptDialogOpen by remember { mutableStateOf(false) }
    var mouseMode by remember { mutableStateOf(MOUSE_MODE_TOUCHPAD) }
    var resolutionPercent by remember { mutableStateOf(100) }
    var scalePercent by remember { mutableStateOf(100) }
    var showKeyboardTrigger by remember { mutableStateOf(0) }
    val prefs = remember(context) { context.getSharedPreferences("trierarch_prefs", 0) }

    LaunchedEffect(Unit) {
        mouseMode = prefs.getInt("mouse_mode", MOUSE_MODE_TOUCHPAD)
        resolutionPercent = prefs.getInt("resolution_percent", 100).coerceIn(10, 100)
        scalePercent = prefs.getInt("scale_percent", 100).coerceIn(10, 100)
    }

    fun persistMouseMode(mode: Int) {
        mouseMode = mode
        prefs.edit().putInt("mouse_mode", mode).apply()
    }

    fun persistResolutionPercent(pct: Int) {
        resolutionPercent = pct.coerceIn(10, 100)
        prefs.edit().putInt("resolution_percent", resolutionPercent).apply()
    }

    fun persistScalePercent(pct: Int) {
        scalePercent = pct.coerceIn(10, 100)
        prefs.edit().putInt("scale_percent", scalePercent).apply()
    }

    // Initialize native layer. MANAGE_EXTERNAL_STORAGE is declared in manifest;
    // users who need file access can grant it manually in Settings.
    LaunchedEffect(Unit) {
        if (NativeBridge.init(
                context.filesDir.absolutePath,
                context.cacheDir.absolutePath,
                context.applicationInfo.nativeLibraryDir,
                Environment.getExternalStorageDirectory()?.absolutePath
            )
        ) {
            initialized = true
            hasRootfs = NativeBridge.hasRootfs()
        } else {
            errorMsg = "Failed to initialize native layer"
        }
    }

    // Spawn proot if rootfs exists, or start download
    LaunchedEffect(initialized, hasRootfs) {
        if (!initialized) return@LaunchedEffect
        if (hasRootfs) {
            if (NativeBridge.spawnProot()) {
                prootSpawned = true
            } else {
                errorMsg = "Failed to spawn proot"
            }
        } else if (!installDone && !downloadStarted) {
            downloadStarted = true
            val mainHandler = Handler(Looper.getMainLooper())
            scope.launch(Dispatchers.IO) {
                val ok = NativeBridge.downloadRootfs(object : ProgressCallback {
                    override fun onProgress(pct: Int, msg: String) {
                        mainHandler.post { installProgress = pct to msg }
                    }
                })
                withContext(Dispatchers.Main) {
                    if (ok) {
                        installDone = true
                    } else {
                        errorMsg = "Download failed"
                        downloadStarted = false
                    }
                }
            }
        }
    }

    val waylandRuntimeDir = remember(context) {
        File(context.filesDir, "usr/tmp").apply { mkdirs() }.absolutePath
    }

    // Immersive fullscreen (hide status/nav bar, swipe from edge to show) when showing Wayland desktop
    LaunchedEffect(showWayland) {
        setImmersiveMode(context as? Activity, showWayland)
    }

    LaunchedEffect(prootSpawned, waylandOn) {
        if (!prootSpawned) return@LaunchedEffect
        if (waylandOn) {
            // Ensure keyboard keymap exists for the Wayland server (Qt relies on it for Ctrl+Shift+U).
            val keymapTarget = File(waylandRuntimeDir, "keymap_us.xkb")
            if (!keymapTarget.exists()) {
                context.assets.open("keymap_us.xkb").use { input ->
                    keymapTarget.outputStream().use { out ->
                        input.copyTo(out)
                    }
                }
            }
            WaylandBridge.nativeStartServer(waylandRuntimeDir)
        } else {
            WaylandBridge.nativeStopWayland()
            showWayland = false
        }
    }

    LaunchedEffect(prootSpawned) {
        if (!prootSpawned) return@LaunchedEffect
        var lastLines = listOf<String>()
        var lastPartial = ""
        while (true) {
            val newLines = NativeBridge.getLines()
            val newPartial = NativeBridge.getPartialLine()
            if (newLines != lastLines || newPartial != lastPartial) {
                lastLines = newLines
                lastPartial = newPartial
                lines = newLines
                partialLine = newPartial
            }
            delay(100)
        }
    }

    // Error state
    errorMsg?.let { msg ->
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            Text(msg, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    // Loading state
    if (!initialized) {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            CircularProgressIndicator(color = MaterialTheme.colorScheme.primary)
        }
        return
    }

    // Init in progress: download + extract
    if (!hasRootfs && !installDone) {
        InstallScreen(
            progress = installProgress.first,
            message = installProgress.second
        )
        return
    }

    // Init done in this session: must restart or exit; terminal is not usable until then
    if (installDone) {
        AlertDialog(
            onDismissRequest = { },
            title = { Text("Setup complete") },
            text = { Text("Restart the app to start the Arch terminal.") },
            confirmButton = {
                TextButton(onClick = {
                    val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)?.apply {
                        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                    }
                    if (intent != null) {
                        context.startActivity(intent)
                        (context as? Activity)?.finish()
                    }
                }) {
                    Text("Restart")
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    (context as? Activity)?.finish()
                }) {
                    Text("Exit")
                }
            }
        )
        return
    }

    // Spawning proot (hasRootfs but not yet prootSpawned)
    if (hasRootfs && !prootSpawned) {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            Text(AppStrings.STARTING_ARCH, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    // Post-init: proot spawned, show terminal or wayland; menu with two-finger swipe from left
    Box(modifier = Modifier.fillMaxSize()) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .twoFingerSwipeFromLeft(onSwipeRight = { menuOpen = true })
        ) {
            if (showWayland) {
                WaylandSurfaceView(
                    runtimeDir = waylandRuntimeDir,
                    mouseMode = mouseMode,
                    resolutionPercent = resolutionPercent,
                    scalePercent = scalePercent,
                    showKeyboardTrigger = showKeyboardTrigger,
                    onKeyboardTriggerConsumed = { showKeyboardTrigger = 0 },
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                TerminalScreen(
                    lines = lines,
                    partialLine = partialLine,
                    inputLine = inputLine,
                    onInputChange = { inputLine = it },
                    onInputSubmit = {
                        NativeBridge.writeInput((it + "\n").toByteArray(Charsets.UTF_8))
                        inputLine = ""
                    }
                )
            }
        }
        SideMenu(
            visible = menuOpen,
            onDismiss = { menuOpen = false },
            waylandOn = waylandOn,
            onWaylandClick = { waylandOn = !waylandOn },
            onDisplayClick = {
                if (showWayland) {
                    showWayland = false
                } else {
                    val hasClients = try { WaylandBridge.nativeHasActiveClients() } catch (_: Throwable) { false }
                    if (!hasClients) {
                        val script = prefs.getString("display_startup_script", "")?.trim()
                        if (!script.isNullOrEmpty()) {
                            NativeBridge.writeInput((script + "\n").toByteArray(Charsets.UTF_8))
                        }
                    }
                    showWayland = true
                }
                menuOpen = false
            },
            onDisplayLongPress = { displayScriptDialogOpen = true },
            onViewClick = { settingsOpen = true },
            onKeyboardClick = {
                menuOpen = false
                showKeyboardTrigger += 1
            }
        )
        if (settingsOpen) {
            ViewSettingsDialog(
                onDismiss = { settingsOpen = false },
                mouseMode = mouseMode,
                resolutionPercent = resolutionPercent,
                scalePercent = scalePercent,
                onMouseModeChange = { persistMouseMode(it) },
                onResolutionPercentChange = { persistResolutionPercent(it) },
                onScalePercentChange = { persistScalePercent(it) }
            )
        }
        if (displayScriptDialogOpen) {
            DisplayScriptDialog(
                initialScript = prefs.getString("display_startup_script", "") ?: "",
                onDismiss = { displayScriptDialogOpen = false },
                onConfirm = { script ->
                    prefs.edit().putString("display_startup_script", script).apply()
                    displayScriptDialogOpen = false
                }
            )
        }
    }
}
