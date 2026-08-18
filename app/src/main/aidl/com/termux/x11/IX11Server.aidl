package com.termux.x11;

import android.os.ParcelFileDescriptor;

/** The only cross-process contract needed for the initial embedded Lorie host. */
interface IX11Server {
    ParcelFileDescriptor getXConnection();
}
