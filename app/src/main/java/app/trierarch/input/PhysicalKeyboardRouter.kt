package app.trierarch.input

import android.view.InputDevice
import android.view.KeyCharacterMap
import android.view.KeyEvent

/**
 * Protocol-neutral description of a key originating from an external physical
 * keyboard. Text entered by an Android IME deliberately does not pass through
 * this type: it has different composing and editing semantics.
 */
data class PhysicalKeyEvent(
    val action: Action,
    val keyCode: Int,
    val scanCode: Int,
    val repeatCount: Int,
    val metaState: Int,
    val deviceId: Int,
    val eventTime: Long,
) {
    enum class Action {
        DOWN,
        UP,
    }

    internal fun identity() = KeyIdentity(deviceId, keyCode)

    internal fun releasedAt(eventTime: Long) = copy(
        action = Action.UP,
        repeatCount = 0,
        eventTime = eventTime,
    )
}

internal data class KeyIdentity(
    val deviceId: Int,
    val keyCode: Int,
)

fun interface PhysicalKeyEventSink {
    /** Returns true when the protocol-specific target accepted the event. */
    fun send(event: PhysicalKeyEvent): Boolean
}

/**
 * Tracks physical key state independently of a desktop protocol.
 *
 * A focused desktop target must call [releaseAll] before it is hidden or loses
 * Android window focus. Otherwise a missing Android ACTION_UP can leave Ctrl,
 * Alt, Shift, or Super logically pressed in the guest session.
 */
class PhysicalKeyboardRouter(
    private val sink: PhysicalKeyEventSink,
) {
    private val pressedKeys = linkedMapOf<KeyIdentity, PhysicalKeyEvent>()
    private val suppressedReleases = mutableSetOf<KeyIdentity>()

    fun dispatch(event: PhysicalKeyEvent): Boolean {
        when (event.action) {
            PhysicalKeyEvent.Action.DOWN -> {
                suppressedReleases.remove(event.identity())
                val accepted = sink.send(event)
                if (event.repeatCount == 0) {
                    if (accepted) pressedKeys[event.identity()] = event
                }
                return accepted
            }

            PhysicalKeyEvent.Action.UP -> {
                if (suppressedReleases.remove(event.identity())) return true
                pressedKeys.remove(event.identity())
            }
        }
        return sink.send(event)
    }

    /** Sends a matching release for every key still considered pressed. */
    fun releaseAll(eventTime: Long): Boolean {
        val releases = pressedKeys.values.map { it.releasedAt(eventTime) }
        pressedKeys.clear()
        suppressedReleases += releases.map { it.identity() }
        return releases.fold(false) { accepted, release -> sink.send(release) || accepted }
    }

    fun hasPressedKeys(): Boolean = pressedKeys.isNotEmpty()

    /**
     * Converts a framework event only when it comes from a non-virtual keyboard
     * device. Android IMEs often manufacture KEYBOARD-source events, so source
     * bits alone are insufficient to classify an event as physical input.
     */
    fun dispatchAndroidEvent(event: KeyEvent): Boolean {
        if (!isPhysicalKeyboard(event)) return false
        if (event.isSystem) return false

        val action = when (event.action) {
            KeyEvent.ACTION_DOWN -> PhysicalKeyEvent.Action.DOWN
            KeyEvent.ACTION_UP -> PhysicalKeyEvent.Action.UP
            else -> return false
        }
        return dispatch(
            PhysicalKeyEvent(
                action = action,
                keyCode = event.keyCode,
                scanCode = event.scanCode,
                repeatCount = event.repeatCount,
                metaState = event.metaState,
                deviceId = event.deviceId,
                eventTime = event.eventTime,
            ),
        )
    }

    companion object {
        fun isPhysicalKeyboard(event: KeyEvent): Boolean {
            if (event.deviceId == KeyCharacterMap.VIRTUAL_KEYBOARD) return false
            val device = event.device ?: return false
            return !device.isVirtual && event.isFromSource(InputDevice.SOURCE_KEYBOARD)
        }
    }
}
