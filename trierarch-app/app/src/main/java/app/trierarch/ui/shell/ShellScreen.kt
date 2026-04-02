package app.trierarch.ui.shell

import android.content.Context
import android.util.TypedValue
import android.view.ViewGroup
import android.view.inputmethod.InputMethodManager
import android.widget.FrameLayout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import app.trierarch.NativeBridge
import app.trierarch.PtyOutputRelay
import app.trierarch.R
import app.trierarch.RustPtySession
import app.trierarch.input.InputRouteState
import app.trierarch.shell.ShellFonts
import app.trierarch.shell.ShellSessionClient
import app.trierarch.shell.ShellViewClient
import com.termux.view.TerminalView

/**
 * PTY shell in a Termux [TerminalView]. Uses status-bar and IME window insets: full usable height when the
 * soft keyboard is hidden, and lays out above the keyboard when it is shown ([AndroidManifest] uses adjustResize).
 */
@Composable
fun ShellScreen(
    terminalFontKey: String,
    activeSessionId: Int,
    terminalSessionIds: List<Int>,
    showKeyboardTrigger: Int,
    onKeyboardTriggerConsumed: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val imm = remember(context) {
        context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    }

    DisposableEffect(Unit) {
        onDispose {
            PtyOutputRelay.unbind()
            InputRouteState.shellTerminalView = null
        }
    }

    AndroidView(
        modifier = modifier
            .fillMaxSize()
            .statusBarsPadding()
            .imePadding(),
        factory = { ctx ->
            val root = FrameLayout(ctx).apply {
                isClickable = false
                isFocusable = false
                descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
            }
            val tv = TerminalView(ctx, null)
            val controller = ShellSessionController(ctx, tv)
            root.setTag(R.id.trierarch_shell_controller, controller)
            tv.setTerminalViewClient(ShellViewClient(tv))
            val textSizePx = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_SP,
                14f,
                ctx.resources.displayMetrics
            ).toInt().coerceAtLeast(1)
            tv.setTextSize(textSizePx)
            tv.setTypeface(ShellFonts.typefaceForPref(ctx, terminalFontKey))
            InputRouteState.shellTerminalView = tv
            tv.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ ->
                tv.post { tv.updateSize() }
            }
            tv.post { tv.updateSize() }
            tv.isFocusable = true
            tv.isFocusableInTouchMode = true
            tv.keepScreenOn = true
            tv.setBackgroundColor(android.graphics.Color.BLACK)
            root.addView(
                tv,
                FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT
                )
            )
            root
        },
        update = { root ->
            val controller = root.getTag(R.id.trierarch_shell_controller) as ShellSessionController
            controller.pruneSessionsExcept(terminalSessionIds.toSet())
            controller.attachSessionIfNeeded(activeSessionId)
            val tv = root.getChildAt(0) as TerminalView
            val applied = root.getTag(R.id.trierarch_terminal_font_applied) as? String
            if (applied != terminalFontKey) {
                root.setTag(R.id.trierarch_terminal_font_applied, terminalFontKey)
                tv.setTypeface(ShellFonts.typefaceForPref(tv.context, terminalFontKey))
            }
            if (showKeyboardTrigger > 0) {
                tv.post {
                    tv.requestFocus()
                    imm.showSoftInput(tv, InputMethodManager.SHOW_IMPLICIT)
                }
                onKeyboardTriggerConsumed()
            }
        }
    )
}

private class ShellSessionController(
    private val context: Context,
    private val terminalView: TerminalView
) {
    private val clients = mutableMapOf<Int, ShellSessionClient>()
    private val sessions = mutableMapOf<Int, RustPtySession>()
    private var attachedId: Int = -1

    fun sessionFor(id: Int): RustPtySession {
        return sessions.getOrPut(id) {
            val client = clients.getOrPut(id) { ShellSessionClient(context, terminalView, id) }
            RustPtySession(context, client, terminalView, id)
        }
    }

    fun attachSessionIfNeeded(id: Int) {
        if (id == attachedId) return
        attachedId = id
        val s = sessionFor(id)
        terminalView.attachSession(s)
        PtyOutputRelay.bind(s, terminalView)
        terminalView.post { terminalView.updateSize() }
    }

    fun pruneSessionsExcept(keep: Set<Int>) {
        val removed = sessions.keys.filter { it !in keep }
        for (id in removed) {
            NativeBridge.closeSession(id)
            sessions.remove(id)
            clients.remove(id)
            PtyOutputRelay.discardSessionQueue(id)
        }
        if (attachedId !in keep) {
            attachedId = -1
        }
    }
}
