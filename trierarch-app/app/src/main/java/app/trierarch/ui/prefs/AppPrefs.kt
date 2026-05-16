package app.trierarch.ui.prefs

import android.content.SharedPreferences

object AppPrefs {
    private const val PREF_LAUNCHER_DEFAULT = "launcher_default_mode"
    private const val PREF_COLD_START_PLATFORM = "launcher_startup_platform"
    private const val PREF_COLD_START_SESSION_TARGET = "launcher_startup_target"

    /** Legacy Arch Wayland script key (unchanged on disk). */
    private const val STORAGE_KEY_ARCH_WAYLAND_STARTUP_SCRIPT = "desktop_startup_script"
    private const val STORAGE_KEY_ARCH_X11_STARTUP_SCRIPT = "arch_x11_desktop_startup_script"
    private const val STORAGE_KEY_DEBIAN_WAYLAND_STARTUP_SCRIPT = "debian_wayland_startup_script"
    /** Legacy Debian X11 script key (unchanged on disk). */
    private const val STORAGE_KEY_DEBIAN_X11_STARTUP_SCRIPT = "debian_desktop_startup_script"
    private const val STORAGE_KEY_LEGACY_CONTAINER_SCRIPT = "container_startup_script"

    /** Stored pref: Arch + Wayland session (legacy). */
    private const val LAUNCHER_PREF_WAYLAND = "Desktop"

    /** Stored pref: Debian + X11 (legacy). */
    private const val LAUNCHER_PREF_X11 = "X11Desktop"

    private const val LAUNCHER_PREF_TERMINAL = "Terminal"

    /** Drawer labels (legacy Arch menu). */
    const val LAUNCHER_MENU_WAYLAND = "Wayland"
    const val LAUNCHER_MENU_TERMINAL = "Terminal"
    const val LAUNCHER_MENU_X11 = "X11"

    fun readColdStartConfig(prefs: SharedPreferences): ColdStartConfig {
        val platformRaw = prefs.getString(PREF_COLD_START_PLATFORM, null)
        val targetRaw = prefs.getString(PREF_COLD_START_SESSION_TARGET, null)
        if (platformRaw != null && targetRaw != null) {
            return normalizeColdStartConfig(
                RootfsPlatform.fromPref(platformRaw),
                targetRaw,
            )
        }
        return migrateLegacyColdStart(prefs.getString(PREF_LAUNCHER_DEFAULT, null))
    }

    fun writeColdStartConfig(prefs: SharedPreferences, config: ColdStartConfig) {
        val normalized = normalizeColdStartConfig(config.platform, config.sessionTarget)
        prefs.edit()
            .putString(PREF_COLD_START_PLATFORM, normalized.platform.prefValue)
            .putString(PREF_COLD_START_SESSION_TARGET, normalized.sessionTarget)
            .apply()
    }

    private fun normalizeColdStartConfig(platform: RootfsPlatform, sessionTarget: String): ColdStartConfig {
        val t = sessionTarget.trim().lowercase()
        return when (platform) {
            RootfsPlatform.WINE -> when {
                t == RootfsSessionMode.WINE_CONTAINER_PICKER.prefValue -> ColdStartConfig.wineContainerPicker()
                t.startsWith(ColdStartConfig.WINE_CONTAINER_ID_PREFIX) -> {
                    val id = t.removePrefix(ColdStartConfig.WINE_CONTAINER_ID_PREFIX).toIntOrNull()
                    if (id != null) ColdStartConfig.wineContainer(id) else ColdStartConfig.wineContainerPicker()
                }
                else -> ColdStartConfig.wineContainerPicker()
            }
            RootfsPlatform.ARCH, RootfsPlatform.DEBIAN -> {
                val mode = RootfsSessionMode.fromPref(t) ?: RootfsSessionMode.TERMINAL
                if (mode == RootfsSessionMode.WINE_CONTAINER_PICKER) {
                    ColdStartConfig.withRootfsSession(platform, RootfsSessionMode.TERMINAL)
                } else {
                    ColdStartConfig.withRootfsSession(platform, mode)
                }
            }
        }
    }

    private fun migrateLegacyColdStart(raw: String?): ColdStartConfig = when (migrateLauncherPref(raw)) {
        LAUNCHER_PREF_X11 -> ColdStartConfig.withRootfsSession(RootfsPlatform.DEBIAN, RootfsSessionMode.X11)
        LAUNCHER_PREF_TERMINAL -> ColdStartConfig.withRootfsSession(RootfsPlatform.ARCH, RootfsSessionMode.TERMINAL)
        else -> ColdStartConfig.withRootfsSession(RootfsPlatform.ARCH, RootfsSessionMode.WAYLAND)
    }

    fun readArchWaylandStartupScript(prefs: SharedPreferences): String =
        prefs.getString(STORAGE_KEY_ARCH_WAYLAND_STARTUP_SCRIPT, "")?.trim().orEmpty()

    fun writeArchWaylandStartupScript(prefs: SharedPreferences, script: String) {
        prefs.edit().putString(STORAGE_KEY_ARCH_WAYLAND_STARTUP_SCRIPT, script).apply()
    }

    fun readArchX11StartupScript(prefs: SharedPreferences): String =
        prefs.getString(STORAGE_KEY_ARCH_X11_STARTUP_SCRIPT, "")?.trim().orEmpty()

    fun writeArchX11StartupScript(prefs: SharedPreferences, script: String) {
        prefs.edit().putString(STORAGE_KEY_ARCH_X11_STARTUP_SCRIPT, script).apply()
    }

    fun readDebianWaylandStartupScript(prefs: SharedPreferences): String =
        prefs.getString(STORAGE_KEY_DEBIAN_WAYLAND_STARTUP_SCRIPT, "")?.trim().orEmpty()

    fun writeDebianWaylandStartupScript(prefs: SharedPreferences, script: String) {
        prefs.edit().putString(STORAGE_KEY_DEBIAN_WAYLAND_STARTUP_SCRIPT, script).apply()
    }

    fun readDebianX11StartupScript(prefs: SharedPreferences): String {
        val primary = prefs.getString(STORAGE_KEY_DEBIAN_X11_STARTUP_SCRIPT, null)?.trim()
        if (!primary.isNullOrEmpty()) return primary
        return prefs.getString(STORAGE_KEY_LEGACY_CONTAINER_SCRIPT, "")?.trim().orEmpty()
    }

    fun writeDebianX11StartupScript(prefs: SharedPreferences, script: String) {
        prefs.edit()
            .putString(STORAGE_KEY_DEBIAN_X11_STARTUP_SCRIPT, script)
            .remove(STORAGE_KEY_LEGACY_CONTAINER_SCRIPT)
            .apply()
    }

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

    @Deprecated("Use readColdStartConfig", ReplaceWith("readColdStartConfig(prefs)"))
    fun readLauncherStartup(prefs: SharedPreferences): ColdStartConfig = readColdStartConfig(prefs)

    @Deprecated("Use writeColdStartConfig", ReplaceWith("writeColdStartConfig(prefs, config)"))
    fun writeLauncherStartup(prefs: SharedPreferences, config: ColdStartConfig) =
        writeColdStartConfig(prefs, config)

    @Deprecated("Use readColdStartConfig", ReplaceWith("readColdStartConfig(prefs)"))
    fun readLauncherDefault(prefs: SharedPreferences): String =
        migrateLauncherPref(prefs.getString(PREF_LAUNCHER_DEFAULT, LAUNCHER_PREF_WAYLAND))

    @Deprecated("Use writeColdStartConfig", ReplaceWith("writeColdStartConfig(prefs, config)"))
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

    /** Shell env injected before user X11 startup scripts (Arch and Debian). */
    fun buildX11ShellEnvSnippet(): String =
        """
        |export DISPLAY=:0
        |unset WAYLAND_DISPLAY 2>/dev/null || true
        |export XDG_SESSION_TYPE=x11
        |
        """.trimMargin()
}
