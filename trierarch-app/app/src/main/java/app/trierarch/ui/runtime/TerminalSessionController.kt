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

    fun addNewInteractiveSession(state: State, rootfs: TerminalSessionIds.RootfsRow): State {
        val next = TerminalSessionIds.nextInteractiveNativeId(state.sessionIds, rootfs)
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

    fun defaultInteractiveSession(rootfs: TerminalSessionIds.RootfsRow): Int = when (rootfs) {
        TerminalSessionIds.RootfsRow.ARCH -> TerminalSessionIds.ARCH_TERMINAL
        TerminalSessionIds.RootfsRow.DEBIAN -> TerminalSessionIds.DEBIAN_TERMINAL
    }

    /** Interactive PTY columns (col ≥ 2) on the given rootfs row. */
    fun interactiveSessions(state: State, rootfs: TerminalSessionIds.RootfsRow): List<Int> {
        val firstCol = TerminalSessionIds.PtyCol.INTERACTIVE_FIRST.index
        return state.sessionIds
            .filter {
                TerminalSessionIds.rootfsRowOf(it) == rootfs &&
                    TerminalSessionIds.ptyColOf(it) >= firstCol
            }
            .sorted()
    }

    /** Session shown in the picker for [rootfs] (active when on that row, else default). */
    fun displaySessionId(state: State, rootfs: TerminalSessionIds.RootfsRow): Int =
        if (TerminalSessionIds.rootfsRowOf(state.activeSessionId) == rootfs) {
            state.activeSessionId
        } else {
            defaultInteractiveSession(rootfs)
        }

    fun sessionPickerOptions(state: State, rootfs: TerminalSessionIds.RootfsRow): List<String> = buildList {
        interactiveSessions(state, rootfs).forEach { add(TerminalSessionIds.sessionPickerLine(it)) }
        add("New session")
        add("Close current session")
    }

    fun handleSessionPickerSelect(
        state: State,
        rootfs: TerminalSessionIds.RootfsRow,
        label: String,
    ): State = when (label) {
        "New session" -> addNewInteractiveSession(state, rootfs)
        "Close current session" -> closeManagedSession(state, rootfs)
        else -> selectFromPickerLine(state, label)
    }

    /** Close the session currently managed for [rootfs]; other rootfs rows are unchanged. */
    fun closeManagedSession(state: State, rootfs: TerminalSessionIds.RootfsRow): State {
        val toClose = displaySessionId(state, rootfs)
        val inRow = interactiveSessions(state, rootfs)
        if (inRow.size <= 1) return state

        val newList = state.sessionIds.filter { it != toClose }
        val newActive = when {
            state.activeSessionId == toClose ->
                inRow.filter { it != toClose }.minOrNull() ?: defaultInteractiveSession(rootfs)
            else -> state.activeSessionId
        }
        return state.copy(sessionIds = newList, activeSessionId = newActive)
    }
}

