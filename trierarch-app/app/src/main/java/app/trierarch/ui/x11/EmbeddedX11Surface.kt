package app.trierarch.ui.x11

import android.app.Activity
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import app.trierarch.X11Runtime
import com.termux.x11.EmbeddedX11Controller
import com.termux.x11.LorieView

/**
 * Embedded X11 surface (LorieView) hosted inside Trierarch's Compose tree.
 *
 * This is intentionally minimal compared to the upstream `com.termux.x11.MainActivity`:
 * - It keeps the X11 server in `:x11` ([app.trierarch.X11Runtime.ensureX11ServerProcessStarted])
 * - It connects to CmdEntryPoint via broadcast + binder fd
 * - It provides basic touch/mouse forwarding so the desktop is usable
 *
 * Extra keys / text input toolbar stays in the legacy activity for now; we can re-home it into the
 * drawer later once the embedded surface is stable.
 */
@Composable
fun EmbeddedX11Surface(
    modifier: Modifier = Modifier,
    visible: Boolean,
    mouseMode: EmbeddedX11Controller.MouseMode,
    onConnectionStateChanged: (connected: Boolean) -> Unit = {},
) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val hostActivity = context as? Activity

    val controller = remember { EmbeddedX11Controller() }

    LaunchedEffect(visible) {
        if (!visible) return@LaunchedEffect
        // Ensure the server is alive; CmdEntryPoint will broadcast ACTION_START when ready.
        X11Runtime.ensureX11ServerProcessStarted(context)
    }

    DisposableEffect(visible, mouseMode) {
        controller.setMouseMode(mouseMode)
        onDispose { }
    }

    DisposableEffect(visible) {
        onDispose {
            controller.detach()
        }
    }

    AndroidView(
        modifier = modifier,
        factory = { ctx ->
            android.widget.FrameLayout(ctx).apply {
                setBackgroundColor(0xFF000000.toInt())
                val lv = LorieView(ctx)
                addView(
                    lv,
                    android.widget.FrameLayout.LayoutParams(
                        android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                        android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                    ),
                )
                hostActivity?.let { act -> controller.attach(act, this, lv) }
            }
        },
        update = { view ->
            // Keep the SurfaceView attached but hidden when not visible to avoid a second native
            // surface layer fighting with Wayland.
            view.visibility = if (visible) android.view.View.VISIBLE else android.view.View.GONE
            if (!visible) {
                controller.detach()
                onConnectionStateChanged(false)
            }
        },
    )
}

