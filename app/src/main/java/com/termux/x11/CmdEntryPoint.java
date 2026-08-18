package com.termux.x11;

import android.os.ParcelFileDescriptor;

/**
 * Exact Java JNI names required by the Lorie binary.  Trierarch runs this only
 * in its isolated {@code :x11} service process.
 */
public final class CmdEntryPoint {
    public static native boolean start(String[] arguments);
    public native ParcelFileDescriptor getXConnection();

    static {
        System.loadLibrary("Xlorie");
    }
}
