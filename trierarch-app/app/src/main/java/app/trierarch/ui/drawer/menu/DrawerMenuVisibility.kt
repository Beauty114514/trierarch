package app.trierarch.ui.drawer.menu

/** Which blocks [DrawerMenu] renders (shared vs distro-specific). */
data class DrawerMenuVisibility(
    val launcherDefault: Boolean = false,
    val graphics: Boolean = false,
    val terminalSettings: Boolean = false,
    val showWaylandEntry: Boolean = false,
    val showX11Entry: Boolean = false,
    val terminalEntry: Boolean = false,
    val desktopView: Boolean = false,
    val keyboard: Boolean = false,
) {
    companion object {
        val TrierarchHub = DrawerMenuVisibility(
            graphics = true,
            terminalSettings = true,
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
