package app.trierarch.input

import android.view.InputDevice
import android.view.KeyCharacterMap
import android.view.KeyEvent
import app.trierarch.WaylandBridge

/**
 * Canonical hardware keyboard router.
 * Handles physical keyboard routing and Alt+CapsLock -> window switch mapping.
 */
class HardwareKeyboardRouter {
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

    private fun isHardwareKeyboardEvent(event: KeyEvent): Boolean {
        val keyCode = event.keyCode
        if (keyCode == KeyEvent.KEYCODE_TAB ||
            keyCode == KeyEvent.KEYCODE_APP_SWITCH ||
            keyCode == KeyEvent.KEYCODE_ALT_LEFT ||
            keyCode == KeyEvent.KEYCODE_ALT_RIGHT ||
            keyCode == KeyEvent.KEYCODE_FUNCTION ||
            keyCode == KeyEvent.KEYCODE_NUM_LOCK ||
            keyCode == KeyEvent.KEYCODE_SCROLL_LOCK
        ) {
            return true
        }
        val source = event.source
        val hasKeyboardSource = (source and InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD
        return hasKeyboardSource || event.deviceId != KeyCharacterMap.VIRTUAL_KEYBOARD
    }

    private fun forwardKey(keyCode: Int, meta: Int, down: Boolean, timeMs: Long) {
        try {
            WaylandBridge.nativeOnKeyEvent(keyCode, meta, down, timeMs)
        } catch (_: Throwable) {
        }
    }

    private fun injectWindowSwitch(timeMs: Long) {
        forwardKey(KeyEvent.KEYCODE_TAB, 0, true, timeMs)
        forwardKey(KeyEvent.KEYCODE_TAB, 0, false, timeMs)
    }

    fun handleHardwareKeyboardEvent(event: KeyEvent): Boolean {
        if (!InputRouteState.waylandVisible) return false
        if (event.action != KeyEvent.ACTION_DOWN && event.action != KeyEvent.ACTION_UP) return false
        if (!isHardwareKeyboardEvent(event)) return false

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
            forwardKey(keyCode, event.metaState, isDown, timeMs)
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
            forwardKey(keyCode, event.metaState, isDown, timeMs)
            return true
        }

        forwardKey(keyCode, event.metaState, isDown, timeMs)
        return true
    }
}

object InputRouteState {
    @Volatile var waylandVisible: Boolean = false
}
