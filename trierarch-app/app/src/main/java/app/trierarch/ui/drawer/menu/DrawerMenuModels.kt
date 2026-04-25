package app.trierarch.ui.drawer.menu

data class DrawerMenuLabels(
    val launcherDefaultLabel: String,
    val desktopVulkanLabel: String,
    val desktopOpenGLLabel: String,
    val terminalFontLabel: String,
    val terminalSessionLabel: String,
    val mouseModeLabel: String,
    val resolutionPercentLabel: String,
    val scalePercentLabel: String,
)

data class DrawerMenuOptions(
    val launcherDefaultOptions: List<String>,
    val desktopVulkanOptions: List<String>,
    val desktopOpenGLOptions: List<String>,
    val terminalFontOptions: List<String>,
    val terminalSessionOptions: List<String>,
    val mouseModeOptions: List<String>,
    val resolutionPercentOptions: List<String>,
    val scalePercentOptions: List<String>,
)

data class DrawerMenuActions(
    val onDesktopClick: () -> Unit,
    val onDesktopLongPress: () -> Unit,
    val onDebianDesktopClick: () -> Unit,
    val onDebianDesktopLongPress: () -> Unit,
    val onTerminalClick: () -> Unit,
    val onViewClick: () -> Unit,
    // Keep old dialog-based actions wired elsewhere; drawer uses dropdowns instead.
    val onAppearanceClick: () -> Unit,
    val onSessionClick: () -> Unit,
    val onKeyboardClick: () -> Unit,
    val onLauncherDefaultSelect: (String) -> Unit,
    val onDesktopVulkanSelect: (String) -> Unit,
    val onDesktopOpenGLSelect: (String) -> Unit,
    val onTerminalFontSelect: (String) -> Unit,
    /** Special values allowed: "New session" and "Close current session". */
    val onTerminalSessionSelect: (String) -> Unit,
    val onMouseModeSelect: (String) -> Unit,
    val onResolutionPercentSelect: (String) -> Unit,
    val onScalePercentSelect: (String) -> Unit,
    val onCloseDrawerRequest: () -> Unit,
)

