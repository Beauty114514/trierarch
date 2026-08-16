package com.termux.view;

import com.termux.terminal.TerminalEmulator;

/**
 * PTY-backed terminal session surface expected by {@link TerminalView}.
 *
 * <p>Termux's {@code TerminalSession} owns its own subprocess. Trierarch instead owns its PTY
 * in native code, so its session supplies the same view-facing operations through this interface.</p>
 */
public interface DisplayableTermSession {
    void write(byte[] data, int offset, int count);

    void write(String data);

    void writeCodePoint(boolean prependEscape, int codePoint);

    TerminalEmulator getEmulator();

    void updateSize(int columns, int rows, int cellWidthPixels, int cellHeightPixels);

    void onCopyTextToClipboard(String text);

    void onPasteTextFromClipboard();
}
