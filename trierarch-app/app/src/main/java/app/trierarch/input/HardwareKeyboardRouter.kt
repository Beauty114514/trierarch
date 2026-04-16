package app.trierarch.input

import android.view.KeyEvent
import app.trierarch.WaylandBridge
import com.termux.view.TerminalView

/**
 * Hardware keyboard router for Wayland clients.
 *
 * Contract:
 * - Only routes events when the Wayland view is visible (see [InputRouteState.waylandVisible]).
 * - Filters out virtual/soft keyboard sources so IME text injection remains handled by the IME sink.
 * - Provides a convenience mapping: Alt + CapsLock -> Alt+Tab window switch sequence.
 *
 * Note: This class intentionally swallows exceptions from JNI calls; if the compositor
 * library is missing or not ready we prefer to fall back to normal Android handling.
 */
class HardwareKeyboardRouter {
    private fun isSystemVolumeKey(keyCode: Int): Boolean {
        return keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
            keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ||
            keyCode == KeyEvent.KEYCODE_VOLUME_MUTE
    }

    private fun isLockKey(keyCode: Int): Boolean {
        return keyCode == KeyEvent.KEYCODE_CAPS_LOCK ||
            keyCode == KeyEvent.KEYCODE_NUM_LOCK ||
            keyCode == KeyEvent.KEYCODE_SCROLL_LOCK
    }

    private fun isModifierKey(keyCode: Int): Boolean {
        return keyCode == KeyEvent.KEYCODE_SHIFT_LEFT ||
            keyCode == KeyEvent.KEYCODE_SHIFT_RIGHT ||
            keyCode == KeyEvent.KEYCODE_CTRL_LEFT ||
            keyCode == KeyEvent.KEYCODE_CTRL_RIGHT ||
            keyCode == KeyEvent.KEYCODE_ALT_LEFT ||
            keyCode == KeyEvent.KEYCODE_ALT_RIGHT ||
            keyCode == KeyEvent.KEYCODE_META_LEFT ||
            keyCode == KeyEvent.KEYCODE_META_RIGHT ||
            keyCode == KeyEvent.KEYCODE_FUNCTION
    }

    private fun forwardKey(keyCode: Int, meta: Int, down: Boolean, timeMs: Long) {
        try {
            WaylandBridge.nativeOnKeyEvent(keyCode, meta, down, timeMs)
        } catch (_: Throwable) {
        }
    }

    // Android repeat: multiple ACTION_DOWN; Wayland expects release between presses.
    private fun forwardHardwareKeyEvent(event: KeyEvent) {
        val keyCode = event.keyCode
        val meta = event.metaState
        val t = event.eventTime
        if (!isModifierKey(keyCode) && !isLockKey(keyCode)) {
            if (event.action != KeyEvent.ACTION_DOWN) return
            forwardKey(keyCode, meta, true, t)
            forwardKey(keyCode, meta, false, t + 1)
            return
        }

        if (event.action == KeyEvent.ACTION_DOWN) {
            forwardKey(keyCode, meta, true, t)
        } else if (event.action == KeyEvent.ACTION_UP) {
            forwardKey(keyCode, meta, false, t)
        }
    }

    private fun injectWindowSwitch(timeMs: Long) {
        forwardKey(KeyEvent.KEYCODE_TAB, 0, true, timeMs)
        forwardKey(KeyEvent.KEYCODE_TAB, 0, false, timeMs)
    }

    fun handleHardwareKeyboardEvent(event: KeyEvent): Boolean {
        if (!InputRouteState.waylandVisible) return false
        if (event.action != KeyEvent.ACTION_DOWN && event.action != KeyEvent.ACTION_UP) return false
        // Let Android handle volume keys so the system volume UI works and AAudio output is controlled
        // via STREAM_MUSIC (see MainActivity.volumeControlStream).
        if (isSystemVolumeKey(event.keyCode)) return false
        if (!HardwareKeyEventPolicy.isLikelyFromHardwareKeyboard(event)) return false

        val isDown = event.action == KeyEvent.ACTION_DOWN
        val keyCode = event.keyCode
        val timeMs = event.eventTime

        // Ignore repeated modifier/lock down events to avoid stuck/toggle storms.
        if (isDown && event.repeatCount > 0 && (isModifierKey(keyCode) || isLockKey(keyCode))) {
            return true
        }

        if (keyCode == KeyEvent.KEYCODE_CAPS_LOCK) {
            val altPressed =
                event.isAltPressed ||
                    (event.metaState and KeyEvent.META_ALT_ON) != 0 ||
                    (event.metaState and KeyEvent.META_ALT_LEFT_ON) != 0 ||
                    (event.metaState and KeyEvent.META_ALT_RIGHT_ON) != 0
            if (isDown && altPressed) {
                injectWindowSwitch(timeMs)
                return true
            }
            forwardHardwareKeyEvent(event)
            return true
        }

        val altPressed =
            event.isAltPressed ||
                (event.metaState and KeyEvent.META_ALT_ON) != 0 ||
                (event.metaState and KeyEvent.META_ALT_LEFT_ON) != 0 ||
                (event.metaState and KeyEvent.META_ALT_RIGHT_ON) != 0
        val isAltTab = keyCode == KeyEvent.KEYCODE_TAB && altPressed
        val isAppSwitch = keyCode == KeyEvent.KEYCODE_APP_SWITCH
        if (isAltTab || isAppSwitch) {
            forwardHardwareKeyEvent(event)
            return true
        }

        forwardHardwareKeyEvent(event)
        return true
    }
}

object InputRouteState {
    @Volatile var waylandVisible: Boolean = false

    /** Active shell [TerminalView]; non-null while the terminal surface is composed. */
    @Volatile var shellTerminalView: TerminalView? = null
}
