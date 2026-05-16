package app.trierarch.ui.prefs

/** Root filesystem / runtime family selected for cold start or drawer actions. */
enum class RootfsPlatform(val prefValue: String, val label: String) {
    ARCH("arch", "Arch"),
    DEBIAN("debian", "Debian"),
    WINE("wine", "Wine"),
    ;

    companion object {
        fun fromPref(value: String?): RootfsPlatform =
            entries.find { it.prefValue == value?.lowercase() } ?: ARCH
    }
}

/** Session kind under [RootfsPlatform.ARCH] or [RootfsPlatform.DEBIAN]; Wine uses container picker / id. */
enum class RootfsSessionMode(val prefValue: String, val label: String) {
    TERMINAL("terminal", "Terminal"),
    WAYLAND("wayland", "Wayland"),
    X11("x11", "X11"),
    WINE_CONTAINER_PICKER("list", "List"),
    ;

    companion object {
        val ARCH_DEBIAN: List<RootfsSessionMode> = listOf(TERMINAL, WAYLAND, X11)

        fun fromPref(value: String?): RootfsSessionMode? {
            val v = value?.lowercase() ?: return null
            if (v.startsWith(ColdStartConfig.WINE_CONTAINER_ID_PREFIX)) return null
            return entries.find { it.prefValue == v }
        }
    }
}

/** Persisted cold-start choice on the Trierarch drawer (platform + session target). */
data class ColdStartConfig(
    val platform: RootfsPlatform,
    /**
     * [RootfsSessionMode.prefValue], or [WINE_CONTAINER_ID_PREFIX] + numeric container id.
     */
    val sessionTarget: String,
) {
    val selectedWineContainerId: Int?
        get() {
            if (!sessionTarget.startsWith(WINE_CONTAINER_ID_PREFIX)) return null
            return sessionTarget.removePrefix(WINE_CONTAINER_ID_PREFIX).toIntOrNull()
        }

    fun opensWineContainerPicker(): Boolean =
        platform == RootfsPlatform.WINE &&
            sessionTarget == RootfsSessionMode.WINE_CONTAINER_PICKER.prefValue

    fun modeLabel(containerNamesById: Map<Int, String>): String = when (platform) {
        RootfsPlatform.WINE -> when {
            opensWineContainerPicker() -> RootfsSessionMode.WINE_CONTAINER_PICKER.label
            selectedWineContainerId != null ->
                containerNamesById[selectedWineContainerId]
                    ?: "Container $selectedWineContainerId"
            else -> RootfsSessionMode.WINE_CONTAINER_PICKER.label
        }
        else -> RootfsSessionMode.fromPref(sessionTarget)?.label ?: RootfsSessionMode.TERMINAL.label
    }

    companion object {
        const val WINE_CONTAINER_ID_PREFIX = "container:"

        fun withRootfsSession(platform: RootfsPlatform, mode: RootfsSessionMode): ColdStartConfig =
            ColdStartConfig(platform, mode.prefValue)

        fun wineContainerPicker(): ColdStartConfig =
            ColdStartConfig(RootfsPlatform.WINE, RootfsSessionMode.WINE_CONTAINER_PICKER.prefValue)

        fun wineContainer(id: Int): ColdStartConfig =
            ColdStartConfig(RootfsPlatform.WINE, "$WINE_CONTAINER_ID_PREFIX$id")
    }
}
