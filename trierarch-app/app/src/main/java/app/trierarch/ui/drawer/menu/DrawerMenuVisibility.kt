package app.trierarch.ui.drawer.menu

/** Which blocks [DrawerMenu] renders (shared vs distro-specific). */
data class DrawerMenuVisibility(
    val launcherDefault: Boolean = false,
    val graphics: Boolean = false,
    val terminalSettings: Boolean = false,
    /** Terminal font dropdown (Appearance); global setting, shown on Trierarch page. */
    val terminalFont: Boolean = true,
    val terminalSession: Boolean = true,
    val showWaylandEntry: Boolean = false,
    val showX11Entry: Boolean = false,
    val terminalEntry: Boolean = false,
    val desktopView: Boolean = false,
    val keyboard: Boolean = false,
    /** When false, Vulkan row is omitted from the built-in graphics block (e.g. shown in [DrawerMenu] desktopHeaderContent). */
    val vulkanDropdown: Boolean = true,
) {
    companion object {
        /** Legacy preset; Trierarch hub uses [TrierarchDrawerPage] instead of [DrawerMenu]. */
        val TrierarchHub = DrawerMenuVisibility(
            graphics = true,
            terminalEntry = true,
        )

        val ArchEnvironment = DrawerMenuVisibility(
            showWaylandEntry = true,
            terminalEntry = true,
            desktopView = true,
            keyboard = true,
        )
    }
}
