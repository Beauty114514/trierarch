package app.trierarch.ui.prefs

import android.content.SharedPreferences

object AppPrefs {
    private const val PREF_LAUNCHER_DEFAULT = "launcher_default_mode"
    private const val PREF_DEBIAN_DESKTOP_SCRIPT = "debian_desktop_startup_script"
    private const val PREF_LEGACY_CONTAINER_SCRIPT = "container_startup_script"

    /** Stored pref: Arch + Wayland session. */
    private const val LAUNCHER_PREF_WAYLAND = "Desktop"

    /** Stored pref: Debian + X11 (Lorie). */
    private const val LAUNCHER_PREF_X11 = "X11Desktop"

    private const val LAUNCHER_PREF_TERMINAL = "Terminal"

    /** Drawer labels (UI). */
    const val LAUNCHER_MENU_WAYLAND = "Wayland"
    const val LAUNCHER_MENU_TERMINAL = "Terminal"
    const val LAUNCHER_MENU_X11 = "X11"

    /**
     * Legacy prefs mapping:
     * - "LLVMPIPE" => Vulkan=LLVMPIPE, OpenGL=LLVMPIPE
     * - "UNIVERSAL"/"VIRGL"/"VENUS" => Vulkan=VENUS, OpenGL=VIRGL
     *
     * @return Pair(migratedModesOrNull, shouldRemoveLegacyKey)
     */
    fun migrateLegacyRendererMode(raw: String): Pair<Pair<String, String>?, Boolean> {
        val u = raw.trim().uppercase()
        if (u.isEmpty()) return Pair(null, false)
        return when (u) {
            "LLVMPIPE" -> Pair(Pair("LLVMPIPE", "LLVMPIPE"), true)
            "UNIVERSAL", "VIRGL", "VENUS" -> Pair(Pair("VENUS", "VIRGL"), true)
            else -> Pair(null, false)
        }
    }

    fun readLauncherDefault(prefs: SharedPreferences): String =
        migrateLauncherPref(prefs.getString(PREF_LAUNCHER_DEFAULT, LAUNCHER_PREF_WAYLAND))

    fun writeLauncherDefault(prefs: SharedPreferences, value: String) {
        prefs.edit().putString(PREF_LAUNCHER_DEFAULT, migrateLauncherPref(value)).apply()
    }

    fun migrateLauncherPref(raw: String?): String = when (raw) {
        null, "" -> LAUNCHER_PREF_WAYLAND
        "Container", "Debian desktop" -> LAUNCHER_PREF_X11
        LAUNCHER_PREF_WAYLAND, LAUNCHER_PREF_TERMINAL, LAUNCHER_PREF_X11 -> raw
        else -> LAUNCHER_PREF_WAYLAND
    }

    fun launcherPrefToMenuLabel(pref: String): String = when (migrateLauncherPref(pref)) {
        LAUNCHER_PREF_WAYLAND -> LAUNCHER_MENU_WAYLAND
        LAUNCHER_PREF_TERMINAL -> LAUNCHER_MENU_TERMINAL
        LAUNCHER_PREF_X11 -> LAUNCHER_MENU_X11
        else -> LAUNCHER_MENU_WAYLAND
    }

    fun menuLabelToLauncherPref(label: String): String = when (label) {
        LAUNCHER_MENU_WAYLAND -> LAUNCHER_PREF_WAYLAND
        LAUNCHER_MENU_TERMINAL -> LAUNCHER_PREF_TERMINAL
        LAUNCHER_MENU_X11 -> LAUNCHER_PREF_X11
        else -> migrateLauncherPref(label)
    }

    fun cycleLauncherDefaultPref(current: String): String = when (migrateLauncherPref(current)) {
        LAUNCHER_PREF_WAYLAND -> LAUNCHER_PREF_X11
        LAUNCHER_PREF_X11 -> LAUNCHER_PREF_TERMINAL
        else -> LAUNCHER_PREF_WAYLAND
    }

    fun readDebianDesktopStartupScript(prefs: SharedPreferences): String {
        val primary = prefs.getString(PREF_DEBIAN_DESKTOP_SCRIPT, null)?.trim()
        if (!primary.isNullOrEmpty()) return primary
        return prefs.getString(PREF_LEGACY_CONTAINER_SCRIPT, "")?.trim().orEmpty()
    }

    fun writeDebianDesktopStartupScript(prefs: SharedPreferences, script: String) {
        prefs.edit()
            .putString(PREF_DEBIAN_DESKTOP_SCRIPT, script)
            .remove(PREF_LEGACY_CONTAINER_SCRIPT)
            .apply()
    }

    fun readInt(prefs: SharedPreferences, key: String, defaultValue: Int): Int =
        prefs.getInt(key, defaultValue)

    fun writeInt(prefs: SharedPreferences, key: String, value: Int) {
        prefs.edit().putInt(key, value).apply()
    }

    fun readString(prefs: SharedPreferences, key: String, defaultValue: String): String =
        prefs.getString(key, defaultValue) ?: defaultValue

    fun writeString(prefs: SharedPreferences, key: String, value: String) {
        prefs.edit().putString(key, value).apply()
    }

    /**
     * Injected into the Debian headless (slot 0) PTY before the user’s script. Proot already sets
     * WAYLAND_DISPLAY etc. in the shell env (see proot/args); X11 clients need DISPLAY and
     * no conflicting Wayland session type.
     */
    fun buildDebianX11ImplicitEnvSnippet(): String =
        """
        |export DISPLAY=:0
        |unset WAYLAND_DISPLAY 2>/dev/null || true
        |export XDG_SESSION_TYPE=x11
        |
        """.trimMargin()
}

