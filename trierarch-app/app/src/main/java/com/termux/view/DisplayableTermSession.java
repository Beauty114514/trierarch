package com.termux.view;

import com.termux.terminal.TerminalEmulator;

/**
 * PTY-backed terminal session surface expected by {@link TerminalView}.
 * <p>
 * The bundled {@link com.termux.terminal.TerminalSession} type is {@code final} and tied to a subprocess
 * spawn path in its default app. Trierarch runs a shell under proot in native code instead, so sessions
 * implement this interface (typically by extending {@link com.termux.terminal.TerminalOutput}).
 */
public interface DisplayableTermSession {
    void write(byte[] data, int offset, int count);

    void writeCodePoint(boolean prependEscape, int codePoint);

    TerminalEmulator getEmulator();

    void updateSize(int columns, int rows);

    /**
     * Satisfied by {@link com.termux.terminal.TerminalOutput#write(String)} on concrete sessions
     * ({@code final} there; must not be re-declared as a {@code default} method here or Kotlin
     * sees conflicting {@code write(String)} implementations).
     */
    void write(String data);

    /** Mirrors {@link com.termux.terminal.TerminalOutput} for text-selection UI. */
    void onCopyTextToClipboard(String text);

    void onPasteTextFromClipboard();
}
