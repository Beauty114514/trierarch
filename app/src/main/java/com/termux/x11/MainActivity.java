package com.termux.x11;

/**
 * Compatibility callback object required by Lorie's fixed JNI lookup.
 * Trierarch owns the visible activity, so these initial display-only callbacks
 * intentionally have no UI behavior.
 */
public final class MainActivity {
    private static final MainActivity INSTANCE = new MainActivity();

    private MainActivity() {}

    public static MainActivity getInstance() {
        return INSTANCE;
    }

    public void clientConnectedStateChanged() {
        // Input and connection state UI are deliberately out of this first slice.
    }
}
