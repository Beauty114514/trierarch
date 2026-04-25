package com.termux.x11;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Small API for Trierarch's Compose UI to read/write the same X11 display keys as {@link Prefs}
 * (backed by {@link android.preference.PreferenceManager}'s default {@code SharedPreferences}).
 */
public final class X11OutputSettings {
    private static final String K_DISPLAY_SCALE = "displayScale";

    private X11OutputSettings() {}

    @NonNull
    public static String getResolutionMode(@Nullable Context ctx) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return "native";
        return p.displayResolutionMode.get();
    }

    public static int getDisplayScalePercent(@Nullable Context ctx) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return 100;
        return p.displayScale.get();
    }

    @NonNull
    public static String getResolutionExact(@Nullable Context ctx) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return "1280x1024";
        return p.displayResolutionExact.get();
    }

    @NonNull
    public static String getResolutionCustom(@Nullable Context ctx) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return "1280x1024";
        return p.displayResolutionCustom.get();
    }

    public static void setResolutionMode(@Nullable Context ctx, @NonNull String mode) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return;
        p.displayResolutionMode.put(mode);
        X11ActivityBridge.refreshLorieFromOutputPreferences(ctx);
    }

    public static void setDisplayScalePercent(@Nullable Context ctx, int percent) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return;
        int v = percent;
        if (v < 30) v = 30;
        if (v > 300) v = 300;
        // Match SeekBarPreference step (10) in res/xml/preferences.xml
        v = (v / 10) * 10;
        p.get().edit().putInt(K_DISPLAY_SCALE, v).commit();
        X11ActivityBridge.refreshLorieFromOutputPreferences(ctx);
    }

    public static void setResolutionExact(@Nullable Context ctx, @NonNull String wxh) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return;
        p.displayResolutionExact.put(wxh);
        X11ActivityBridge.refreshLorieFromOutputPreferences(ctx);
    }

    public static void setResolutionCustom(@Nullable Context ctx, @NonNull String wxh) {
        Prefs p = X11ActivityBridge.getOrCreatePrefs(ctx);
        if (p == null) return;
        p.displayResolutionCustom.put(wxh.trim());
        X11ActivityBridge.refreshLorieFromOutputPreferences(ctx);
    }
}
