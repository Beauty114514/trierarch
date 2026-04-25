package app.trierarch.ui.runtime

import app.trierarch.TerminalSessionIds

/**
 * Pure session list management for the terminal UI.
 *
 * Contract:
 * - Does not touch native PTY lifecycle (spawn/close); it only manages the UI-visible list.
 * - Callers decide how to react when the active session changes (focus, routing, etc.).
 */
object TerminalSessionController {

    data class State(
        val sessionIds: List<Int>,
        val activeSessionId: Int,
    )

    fun initialState(): State = State(
        sessionIds = listOf(
            TerminalSessionIds.ARCH_TERMINAL,
            TerminalSessionIds.DEBIAN_TERMINAL,
            TerminalSessionIds.WINE_TERMINAL,
        ),
        activeSessionId = TerminalSessionIds.ARCH_TERMINAL,
    )

    fun selectIfPresent(state: State, nativeSessionId: Int): State {
        if (nativeSessionId !in state.sessionIds) return state
        if (nativeSessionId == state.activeSessionId) return state
        return state.copy(activeSessionId = nativeSessionId)
    }

    fun selectFromPickerLine(state: State, pickerLine: String): State {
        val id = TerminalSessionIds.parseSessionPickerLine(pickerLine) ?: return state
        return selectIfPresent(state, id)
    }

    fun addNewInteractiveSession(state: State, namespace: Int): State {
        val next = TerminalSessionIds.nextInteractiveNativeId(state.sessionIds, namespace)
        return state.copy(
            sessionIds = state.sessionIds + next,
            activeSessionId = next,
        )
    }

    fun closeCurrentSession(state: State): State {
        val id = state.activeSessionId
        if (state.sessionIds.size <= 1) return state

        val newList = state.sessionIds.filter { it != id }
        val newActive = if (id == state.activeSessionId) newList.first() else state.activeSessionId
        return state.copy(
            sessionIds = newList,
            activeSessionId = newActive,
        )
    }
}

