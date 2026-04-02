package app.trierarch.input

import android.view.InputDevice
import android.view.KeyCharacterMap
import android.view.KeyEvent

/** Classifies [KeyEvent]s for hardware keyboard routing (excludes typical IME-generated events). */
object HardwareKeyEventPolicy {
    fun isLikelyFromHardwareKeyboard(event: KeyEvent): Boolean {
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
}
