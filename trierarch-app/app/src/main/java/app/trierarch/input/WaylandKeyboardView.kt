package app.trierarch.input

import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import app.trierarch.WaylandBridge
import java.util.concurrent.Executors

/**
 * Focusable view that receives soft keyboard input and forwards key events
 * to the Wayland compositor (approach A: Android as keyboard).
 * For characters that cannot be produced by a normal keymap (e.g. CJK/emoji),
 * inject Linux desktop Unicode input sequence: Ctrl+Shift+U + hex + Space (per codepoint).
 * Measured 1x1 so IME will show; positioned off-screen so it does not block touch.
 */
class WaylandKeyboardView(context: android.content.Context) : View(context) {
    private val commitExecutor = Executors.newSingleThreadExecutor { r ->
        Thread(r, "WaylandCommitExecutor").apply { isDaemon = true }
    }

    init {
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        setMeasuredDimension(1, 1)
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = EditorInfo.TYPE_NULL
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

            override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                if (text.isNullOrEmpty()) return true
                val t = text.toString()
                // Never block the IME/Ui thread with long paste; inject asynchronously.
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
                            10 -> { // \n
                                tap(KeyEvent.KEYCODE_ENTER)
                                continue
                            }
                            9 -> { // \t
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

                        // Unicode input: Ctrl+Shift+U then hex digits then Space (confirm).
                        keyDown(KeyEvent.KEYCODE_CTRL_LEFT)
                        keyDown(KeyEvent.KEYCODE_SHIFT_LEFT)
                        tap(KeyEvent.KEYCODE_U)
                        keyUp(KeyEvent.KEYCODE_SHIFT_LEFT)
                        keyUp(KeyEvent.KEYCODE_CTRL_LEFT)

                        val hex = cp.toString(16) // lowercase
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
                        // Yield a bit during long pastes to keep UI/render responsive.
                        if (longPaste) {
                            // For very long pastes, slow down noticeably to avoid overwhelming Wayland.
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

    /** Map printable ASCII to Android keycode and shift; 0 = unmapped (e.g. CJK, skip). */
    private fun charToAndroidKeyCode(c: Char): Pair<Int, Boolean> {
        val code = c.code
        when {
            code in 48..57 -> return Pair(KeyEvent.KEYCODE_0 + (code - 48), false)  // 0-9
            code in 97..122 -> return Pair(KeyEvent.KEYCODE_A + (code - 97), false)  // a-z
            code in 65..90 -> return Pair(KeyEvent.KEYCODE_A + (code - 65), true)    // A-Z
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
            code in 33..47 -> return Pair(symbolToKeyCode(code), true)   // !"#$%&'()*+,-./
            code in 58..64 -> return Pair(symbolToKeyCode(code), true)   // :;<=>?@
            code in 123..126 -> return Pair(symbolToKeyCode(code), true) // {|}~
            else -> return Pair(0, false)
        }
    }

    /** Only called for shift-needed symbols: 33..43 (! to +), 58,60,62..64 (: < > ? @), 123..126 ({ | } ~). */
    private fun symbolToKeyCode(code: Int): Int = when (code) {
        33 -> KeyEvent.KEYCODE_1      // !
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
        58 -> KeyEvent.KEYCODE_SEMICOLON  // :
        60 -> KeyEvent.KEYCODE_COMMA      // <
        62 -> KeyEvent.KEYCODE_PERIOD     // >
        63 -> KeyEvent.KEYCODE_SLASH      // ?
        64 -> KeyEvent.KEYCODE_2          // @
        94 -> KeyEvent.KEYCODE_6          // ^
        95 -> KeyEvent.KEYCODE_MINUS      // _
        123 -> KeyEvent.KEYCODE_LEFT_BRACKET   // {
        124 -> KeyEvent.KEYCODE_BACKSLASH      // |
        125 -> KeyEvent.KEYCODE_RIGHT_BRACKET  // }
        126 -> KeyEvent.KEYCODE_GRAVE     // ~
        else -> 0
    }
}
