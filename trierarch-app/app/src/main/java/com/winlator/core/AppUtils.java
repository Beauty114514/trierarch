package com.winlator.core;

import android.os.Environment;

/** Minimal path constants used by upstream {@code Container}. */
public final class AppUtils {
    public static final String INTERNAL_STORAGE = Environment.getExternalStorageDirectory().getPath();
    public static final String DIRECTORY_DOWNLOADS = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getPath();

    private AppUtils() {}
}

