package app.trierarch.input

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PhysicalKeyboardRouterTest {
    @Test
    fun `releaseAll releases each held key once`() {
        val sent = mutableListOf<PhysicalKeyEvent>()
        val router = PhysicalKeyboardRouter { event -> sent += event; true }
        val control = event(PhysicalKeyEvent.Action.DOWN, keyCode = 113, scanCode = 29)
        val c = event(PhysicalKeyEvent.Action.DOWN, keyCode = 31, scanCode = 46)

        router.dispatch(control)
        router.dispatch(c)

        assertTrue(router.releaseAll(eventTime = 90))
        assertFalse(router.hasPressedKeys())
        assertEquals(
            listOf(
                control,
                c,
                control.copy(action = PhysicalKeyEvent.Action.UP, repeatCount = 0, eventTime = 90),
                c.copy(action = PhysicalKeyEvent.Action.UP, repeatCount = 0, eventTime = 90),
            ),
            sent,
        )
    }

    @Test
    fun `repeat does not create a second held key`() {
        val sent = mutableListOf<PhysicalKeyEvent>()
        val router = PhysicalKeyboardRouter { event -> sent += event; true }
        val down = event(PhysicalKeyEvent.Action.DOWN, keyCode = 29, scanCode = 30)

        router.dispatch(down)
        router.dispatch(down.copy(repeatCount = 2, eventTime = 40))
        router.releaseAll(eventTime = 80)

        assertEquals(
            listOf(
                down,
                down.copy(repeatCount = 2, eventTime = 40),
                down.copy(action = PhysicalKeyEvent.Action.UP, repeatCount = 0, eventTime = 80),
            ),
            sent,
        )
    }

    @Test
    fun `keys from separate devices are released independently`() {
        val sent = mutableListOf<PhysicalKeyEvent>()
        val router = PhysicalKeyboardRouter { event -> sent += event; true }

        router.dispatch(event(PhysicalKeyEvent.Action.DOWN, keyCode = 59, scanCode = 42, deviceId = 3))
        router.dispatch(event(PhysicalKeyEvent.Action.DOWN, keyCode = 59, scanCode = 42, deviceId = 7))
        router.dispatch(event(PhysicalKeyEvent.Action.UP, keyCode = 59, scanCode = 42, deviceId = 3))
        router.releaseAll(eventTime = 100)

        assertEquals(4, sent.size)
        assertEquals(7, sent.last().deviceId)
        assertEquals(PhysicalKeyEvent.Action.UP, sent.last().action)
    }

    @Test
    fun `late release after a focus reset is not delivered to a new target`() {
        val sent = mutableListOf<PhysicalKeyEvent>()
        val router = PhysicalKeyboardRouter { event -> sent += event; true }
        val down = event(PhysicalKeyEvent.Action.DOWN, keyCode = 57, scanCode = 56)

        router.dispatch(down)
        router.releaseAll(eventTime = 50)
        assertTrue(router.dispatch(down.copy(action = PhysicalKeyEvent.Action.UP, eventTime = 60)))

        assertEquals(2, sent.size)
        assertEquals(PhysicalKeyEvent.Action.UP, sent.last().action)
        assertEquals(50, sent.last().eventTime)
    }

    private fun event(
        action: PhysicalKeyEvent.Action,
        keyCode: Int,
        scanCode: Int,
        repeatCount: Int = 0,
        deviceId: Int = 1,
        eventTime: Long = 10,
    ) = PhysicalKeyEvent(
        action = action,
        keyCode = keyCode,
        scanCode = scanCode,
        repeatCount = repeatCount,
        metaState = 0,
        deviceId = deviceId,
        eventTime = eventTime,
    )
}
