package app.trierarch.wayland

import android.view.Gravity
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.viewinterop.AndroidView
import app.trierarch.WaylandBridge
import app.trierarch.input.SoftKeyboardView

@Composable
fun WaylandSurfaceView(
    runtimeDir: String,
    mouseMode: Int,
    resolutionPercent: Int,
    scalePercent: Int,
    /** Keep false (see `WaylandBridge.nativeSurfaceCreated` javadoc). */
    skipEglWaylandBind: Boolean,
    showKeyboardTrigger: Int,
    keyboardWanted: Boolean,
    onKeyboardTriggerConsumed: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    /*
     * Thin host wrapper for the embedded Wayland compositor output:
     * - Surface lifecycle -> JNI calls
     * - Compose state -> [WaylandTouchLayout] state
     *
     * Input/gesture logic, soft keyboard recovery, and cursor policies live in:
     * - `WaylandTouchLayout.kt`
     * - `WaylandImeRecovery.kt`
     * - `WaylandCursorVisibilityPolicy.kt`
     */
    val rp = resolutionPercent.coerceIn(10, 100)
    val sp = scalePercent.coerceIn(100, 1000)
    val lifecycleOwner = LocalLifecycleOwner.current

    AndroidView(
        factory = { ctx ->
            val layout = WaylandTouchLayout(ctx).apply {
                this.resolutionPercent = rp
                this.scalePercent = sp
                bindLifecycleOwner(lifecycleOwner)
            }

            val surfaceView = SurfaceView(ctx).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(h: SurfaceHolder) {
                        val surface = h.surface ?: return
                        val l = (this@apply.parent as? WaylandTouchLayout)
                        val r = l?.resolutionPercent?.coerceIn(10, 100) ?: 100
                        val s = l?.scalePercent?.coerceIn(100, 1000) ?: 100
                        WaylandBridge.nativeSurfaceCreated(surface, runtimeDir, r, s, skipEglWaylandBind)
                        l?.applyCursorVisibilityPolicy()
                    }

                    override fun surfaceChanged(h: SurfaceHolder, format: Int, width: Int, height: Int) {
                        if (width <= 0 || height <= 0) return
                        val l = (this@apply.parent as? WaylandTouchLayout) ?: return
                        l.lastSurfaceWidth = width
                        l.lastSurfaceHeight = height
                        l.onSurfaceSizeChanged(width, height)
                        val r = l.resolutionPercent.coerceIn(10, 100)
                        val s = l.scalePercent.coerceIn(100, 1000)
                        WaylandBridge.nativeOutputSizeChanged(width, height, r, s)
                        l.lastAppliedResolutionPercent = r
                        l.lastAppliedScalePercent = s
                    }

                    override fun surfaceDestroyed(h: SurfaceHolder) {
                        WaylandBridge.nativeSurfaceDestroyed()
                    }
                })
            }

            val keyboardView = SoftKeyboardView(ctx)
            layout.addView(
                surfaceView,
                FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    Gravity.CENTER
                )
            )
            layout.addView(keyboardView, FrameLayout.LayoutParams(1, 1).apply {
                gravity = Gravity.TOP or Gravity.END
                marginEnd = -1
            })
            keyboardView.post { keyboardView.requestFocus() }
            layout.keyboardSinkView = keyboardView
            layout
        },
        update = { view ->
            val layout = view as? WaylandTouchLayout ?: return@AndroidView
            layout.bindLifecycleOwner(lifecycleOwner)
            layout.mouseMode = mouseMode
            layout.resolutionPercent = rp
            layout.scalePercent = sp
            layout.setKeyboardWanted(keyboardWanted)
            layout.applyCursorVisibilityPolicy()

            if (layout.lastSurfaceWidth > 0 && layout.lastSurfaceHeight > 0 &&
                (layout.lastAppliedResolutionPercent != rp || layout.lastAppliedScalePercent != sp)
            ) {
                layout.onSurfaceSizeChanged(layout.lastSurfaceWidth, layout.lastSurfaceHeight)
                WaylandBridge.nativeOutputSizeChanged(layout.lastSurfaceWidth, layout.lastSurfaceHeight, rp, sp)
                layout.lastAppliedResolutionPercent = rp
                layout.lastAppliedScalePercent = sp
            }

            if (showKeyboardTrigger > 0) {
                // Keyboard menu click: set wanted=true at the App level, then call show once here.
                layout.ensureSoftKeyboardVisible()
                onKeyboardTriggerConsumed()
            }
        },
        modifier = modifier
    )
}

