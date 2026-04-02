package app.trierarch

/** Native PTY session ids: 0 = headless shell for Display startup script; 1+ = terminal tabs. */
object TerminalSessionIds {
    const val DISPLAY: Int = 0
    const val FIRST_TERMINAL: Int = 1
}
