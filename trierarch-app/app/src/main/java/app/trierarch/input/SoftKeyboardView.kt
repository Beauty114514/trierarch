package app.trierarch.input

import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import app.trierarch.WaylandBridge
import java.util.concurrent.Executors

/**
 * Soft keyboard input sink (IME path) for Wayland clients.
 *
 * Contract:
 * - Android IME delivers either key events (hardware-like) or committed text.
 * - We translate both into Wayland keyboard events via [WaylandBridge.nativeOnKeyEvent].
 * - For non-ASCII text, we inject Unicode using the de-facto X11/GTK/Qt path:
 *   Ctrl+Shift+U, then hex codepoint, then Space. This works broadly across toolkits
 *   and avoids needing a dedicated text-input protocol implementation.
 *
 * Threading:
 * - commitText() work is offloaded to a single background executor to keep IME/UI responsive.
 * - Native code further handles any libwayland-server thread-safety constraints.
 */
class SoftKeyboardView(context: android.content.Context) : View(context) {
    private val commitExecutor = Executors.newSingleThreadExecutor { r ->
        Thread(r, "WaylandCommitExecutor").apply { isDaemon = true }
    }

    init {
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        setMeasuredDimension(1, 1)
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        /*
         * Treat this view as a real text editor target for IMEs.
         *
         * Some keyboards (especially during IME switching) require a focused view to report itself
         * as a text editor with a non-NULL inputType before they will show reliably.
         */
        outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN
        return object : BaseInputConnection(this, true) {
            override fun sendKeyEvent(event: KeyEvent): Boolean {
                try {
                    WaylandBridge.nativeOnKeyEvent(
                        event.keyCode,
                        event.metaState,
                        event.action == KeyEvent.ACTION_DOWN,
                        event.eventTime
                    )
                } catch (_: Throwable) { }
                return true
            }

            override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
                val before = beforeLength.coerceAtLeast(0).coerceAtMost(256)
                val after = afterLength.coerceAtLeast(0).coerceAtMost(256)
                if (before > 0 || after > 0) {
                    commitExecutor.execute {
                        var time = System.currentTimeMillis()
                        repeat(before) {
                            WaylandBridge.nativeOnKeyEvent(KeyEvent.KEYCODE_DEL, 0, true, time)
                            time++
                            WaylandBridge.nativeOnKeyEvent(KeyEvent.KEYCODE_DEL, 0, false, time)
                            time++
                        }
                        repeat(after) {
                            WaylandBridge.nativeOnKeyEvent(KeyEvent.KEYCODE_FORWARD_DEL, 0, true, time)
                            time++
                            WaylandBridge.nativeOnKeyEvent(KeyEvent.KEYCODE_FORWARD_DEL, 0, false, time)
                            time++
                        }
                    }
                }
                return super.deleteSurroundingText(beforeLength, afterLength)
            }

            override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                if (text.isNullOrEmpty()) return true
                val t = text.toString()
                commitExecutor.execute {
                    var time = System.currentTimeMillis()
                    fun keyDown(keyCode: Int) {
                        WaylandBridge.nativeOnKeyEvent(keyCode, 0, true, time)
                        time++
                    }
                    fun keyUp(keyCode: Int) {
                        WaylandBridge.nativeOnKeyEvent(keyCode, 0, false, time)
                        time++
                    }
                    fun tap(keyCode: Int) {
                        keyDown(keyCode)
                        keyUp(keyCode)
                    }

                    val cps = t.codePoints().toArray()
                    var injectedCodepoints = 0
                    val longPaste = cps.size >= 200
                    for (cp in cps) {
                        when (cp) {
                            10 -> {
                                tap(KeyEvent.KEYCODE_ENTER)
                                continue
                            }
                            9 -> {
                                tap(KeyEvent.KEYCODE_TAB)
                                continue
                            }
                        }

                        if (cp in 32..126) {
                            val (keyCode, shift) = charToAndroidKeyCode(cp.toChar())
                            if (keyCode != 0) {
                                if (shift) keyDown(KeyEvent.KEYCODE_SHIFT_LEFT)
                                tap(keyCode)
                                if (shift) keyUp(KeyEvent.KEYCODE_SHIFT_LEFT)
                                continue
                            }
                        }

                        keyDown(KeyEvent.KEYCODE_CTRL_LEFT)
                        keyDown(KeyEvent.KEYCODE_SHIFT_LEFT)
                        tap(KeyEvent.KEYCODE_U)
                        keyUp(KeyEvent.KEYCODE_SHIFT_LEFT)
                        keyUp(KeyEvent.KEYCODE_CTRL_LEFT)

                        val hex = cp.toString(16)
                        for (ch in hex) {
                            val (kc, sh) = charToAndroidKeyCode(ch)
                            if (kc != 0) {
                                if (sh) keyDown(KeyEvent.KEYCODE_SHIFT_LEFT)
                                tap(kc)
                                if (sh) keyUp(KeyEvent.KEYCODE_SHIFT_LEFT)
                            }
                        }
                        tap(KeyEvent.KEYCODE_SPACE)

                        injectedCodepoints++
                        if (longPaste) {
                            if (injectedCodepoints % 8 == 0) {
                                try { Thread.sleep(2) } catch (_: InterruptedException) { }
                            }
                        } else if (injectedCodepoints % 64 == 0) {
                            try { Thread.sleep(1) } catch (_: InterruptedException) { }
                        }
                    }
                }
                return true
            }
        }
    }

    private fun charToAndroidKeyCode(c: Char): Pair<Int, Boolean> {
        val code = c.code
        when {
            code in 48..57 -> return Pair(KeyEvent.KEYCODE_0 + (code - 48), false)
            code in 97..122 -> return Pair(KeyEvent.KEYCODE_A + (code - 97), false)
            code in 65..90 -> return Pair(KeyEvent.KEYCODE_A + (code - 65), true)
            code == 32 -> return Pair(KeyEvent.KEYCODE_SPACE, false)
            code == 10 -> return Pair(KeyEvent.KEYCODE_ENTER, false)
            code == 9 -> return Pair(KeyEvent.KEYCODE_TAB, false)
            code == 45 -> return Pair(KeyEvent.KEYCODE_MINUS, false)
            code == 61 -> return Pair(KeyEvent.KEYCODE_EQUALS, false)
            code == 91 -> return Pair(KeyEvent.KEYCODE_LEFT_BRACKET, false)
            code == 93 -> return Pair(KeyEvent.KEYCODE_RIGHT_BRACKET, false)
            code == 59 -> return Pair(KeyEvent.KEYCODE_SEMICOLON, false)
            code == 39 -> return Pair(KeyEvent.KEYCODE_APOSTROPHE, false)
            code == 44 -> return Pair(KeyEvent.KEYCODE_COMMA, false)
            code == 46 -> return Pair(KeyEvent.KEYCODE_PERIOD, false)
            code == 47 -> return Pair(KeyEvent.KEYCODE_SLASH, false)
            code == 92 -> return Pair(KeyEvent.KEYCODE_BACKSLASH, false)
            code == 96 -> return Pair(KeyEvent.KEYCODE_GRAVE, false)
            code in 33..47 -> return Pair(symbolToKeyCode(code), true)
            code in 58..64 -> return Pair(symbolToKeyCode(code), true)
            code in 94..95 -> return Pair(symbolToKeyCode(code), true)
            code in 123..126 -> return Pair(symbolToKeyCode(code), true)
            else -> return Pair(0, false)
        }
    }

    private fun symbolToKeyCode(code: Int): Int = when (code) {
        33 -> KeyEvent.KEYCODE_1
        34 -> KeyEvent.KEYCODE_APOSTROPHE
        35 -> KeyEvent.KEYCODE_3
        36 -> KeyEvent.KEYCODE_4
        37 -> KeyEvent.KEYCODE_5
        38 -> KeyEvent.KEYCODE_7
        39 -> KeyEvent.KEYCODE_APOSTROPHE
        40 -> KeyEvent.KEYCODE_9
        41 -> KeyEvent.KEYCODE_0
        42 -> KeyEvent.KEYCODE_8
        43 -> KeyEvent.KEYCODE_EQUALS
        58 -> KeyEvent.KEYCODE_SEMICOLON
        60 -> KeyEvent.KEYCODE_COMMA
        62 -> KeyEvent.KEYCODE_PERIOD
        63 -> KeyEvent.KEYCODE_SLASH
        64 -> KeyEvent.KEYCODE_2
        94 -> KeyEvent.KEYCODE_6
        95 -> KeyEvent.KEYCODE_MINUS
        123 -> KeyEvent.KEYCODE_LEFT_BRACKET
        124 -> KeyEvent.KEYCODE_BACKSLASH
        125 -> KeyEvent.KEYCODE_RIGHT_BRACKET
        126 -> KeyEvent.KEYCODE_GRAVE
        else -> 0
    }
}
