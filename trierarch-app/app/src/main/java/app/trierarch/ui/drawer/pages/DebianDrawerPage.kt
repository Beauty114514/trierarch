package app.trierarch.ui.drawer.pages

import android.content.SharedPreferences
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.animation.togetherWith
import androidx.compose.material3.DrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import app.trierarch.TerminalSessionIds
import app.trierarch.ui.dialog.MOUSE_MODE_TABLET
import app.trierarch.ui.drawer.menu.DisplayProtocol
import app.trierarch.ui.drawer.menu.DrawerDesktopLaunchButton
import app.trierarch.ui.drawer.menu.DrawerEmbeddedX11ViewSettings
import app.trierarch.ui.drawer.menu.DrawerMenu
import app.trierarch.ui.drawer.menu.DrawerMenuActions
import app.trierarch.ui.drawer.menu.DrawerMenuLabels
import app.trierarch.ui.drawer.menu.DrawerMenuOptions
import app.trierarch.ui.drawer.menu.DrawerMenuVisibility
import app.trierarch.ui.drawer.menu.DrawerProtocolRow
import app.trierarch.ui.drawer.menu.DrawerScriptEditor
import app.trierarch.ui.drawer.menu.DrawerWaylandViewSettings
import app.trierarch.ui.prefs.AppPrefs
import app.trierarch.ui.runtime.TerminalSessionController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

@Composable
fun DebianDrawerPage(
    prefs: SharedPreferences,
    drawerState: DrawerState,
    scope: CoroutineScope,
    terminalSessionState: TerminalSessionController.State,
    onTerminalSessionStateChange: (TerminalSessionController.State) -> Unit,
    desktopVulkanMode: String,
    desktopOpenGLMode: String,
    mouseMode: Int,
    resolutionPercent: Int,
    scalePercent: Int,
    x11MouseModeLabel: String,
    onX11MouseModeSelectLabel: (String) -> Unit,
    x11ResolutionModeLabel: String,
    onX11ResolutionModeSelectLabel: (String) -> Unit,
    x11DisplayScaleLabel: String,
    onX11DisplayScaleSelectLabel: (String) -> Unit,
    x11ResolutionExactLabel: String,
    onX11ResolutionExactSelectLabel: (String) -> Unit,
    x11ResolutionCustom: String,
    onX11ResolutionCustomChange: (String) -> Unit,
    onX11ResolutionCustomApply: () -> Unit,
    debianWaylandScriptEditorOpen: Boolean,
    onDebianWaylandScriptEditorOpenChange: (Boolean) -> Unit,
    debianX11ScriptEditorOpen: Boolean,
    onDebianX11ScriptEditorOpenChange: (Boolean) -> Unit,
    onEnterDebianWayland: () -> Unit,
    onEnterDebianX11: () -> Unit,
    onEnterTerminal: () -> Unit,
    onLeaveGraphicalSurface: () -> Unit,
    onShowKeyboard: () -> Unit,
    onDesktopVulkanSelect: (String) -> Unit,
    onDesktopOpenGLSelect: (String) -> Unit,
    onWaylandMouseModeSelectLabel: (String) -> Unit,
    onWaylandResolutionPercentSelectLabel: (String) -> Unit,
    onWaylandScalePercentSelectLabel: (String) -> Unit,
    vulkanOptions: List<String>,
    openGLOptions: List<String>,
) {
    var displayProtocol by remember { mutableStateOf(DisplayProtocol.WAYLAND) }
    val scriptEditorOpen = debianWaylandScriptEditorOpen || debianX11ScriptEditorOpen
    val waylandMouseModeLabel =
        if (mouseMode == MOUSE_MODE_TABLET) "Tablet" else "Touchpad"
    val waylandResolutionLabel = "${resolutionPercent.coerceIn(10, 100)}%"
    val waylandScaleLabel = "${scalePercent.coerceIn(100, 1000)}%"

    fun closeScriptEditors() {
        onDebianWaylandScriptEditorOpenChange(false)
        onDebianX11ScriptEditorOpenChange(false)
    }

    DrawerMenu(
        title = "Debian",
        labels = DrawerMenuLabels(
            launcherDefaultLabel = "",
            desktopVulkanLabel = desktopVulkanMode,
            desktopOpenGLLabel = desktopOpenGLMode,
            terminalFontLabel = "",
            terminalSessionLabel = "",
            mouseModeLabel = waylandMouseModeLabel,
            resolutionPercentLabel = waylandResolutionLabel,
            scalePercentLabel = waylandScaleLabel,
        ),
        options = DrawerMenuOptions(
            launcherDefaultOptions = emptyList(),
            desktopVulkanOptions = vulkanOptions,
            desktopOpenGLOptions = openGLOptions,
            terminalFontOptions = emptyList(),
            terminalSessionOptions = emptyList(),
            mouseModeOptions = emptyList(),
            resolutionPercentOptions = emptyList(),
            scalePercentOptions = emptyList(),
        ),
        actions = DrawerMenuActions(
            onWaylandEntryClick = { },
            onWaylandEntryLongPress = { },
            onX11EntryClick = { },
            onX11EntryLongPress = { },
            onTerminalClick = {
                scope.launch { drawerState.close() }
                onTerminalSessionStateChange(
                    terminalSessionState.copy(activeSessionId = TerminalSessionIds.DEBIAN_TERMINAL),
                )
                onLeaveGraphicalSurface()
                onEnterTerminal()
            },
            onViewClick = { scope.launch { drawerState.close() } },
            onAppearanceClick = { scope.launch { drawerState.close() } },
            onSessionClick = { scope.launch { drawerState.close() } },
            onKeyboardClick = {
                scope.launch { drawerState.close() }
                onShowKeyboard()
            },
            onLauncherDefaultSelect = { },
            onDesktopVulkanSelect = onDesktopVulkanSelect,
            onDesktopOpenGLSelect = onDesktopOpenGLSelect,
            onTerminalFontSelect = { },
            onTerminalSessionSelect = { },
            onMouseModeSelect = { },
            onResolutionPercentSelect = { },
            onScalePercentSelect = { },
            onCloseDrawerRequest = { scope.launch { drawerState.close() } },
        ),
        visibility = DrawerMenuVisibility(
            launcherDefault = false,
            graphics = true,
            terminalSettings = false,
            showWaylandEntry = false,
            showX11Entry = false,
            terminalEntry = true,
            desktopView = true,
            keyboard = true,
        ),
        desktopHeaderContent = {
            val headerMode = when {
                debianWaylandScriptEditorOpen -> "wayland-script"
                debianX11ScriptEditorOpen -> "x11-script"
                else -> "protocol"
            }
            AnimatedContent(
                targetState = headerMode,
                transitionSpec = {
                    (fadeIn() + expandVertically()).togetherWith(fadeOut() + shrinkVertically())
                },
                label = "Debian protocol script editor",
            ) { mode ->
                when (mode) {
                    "wayland-script" -> {
                        DrawerScriptEditor(
                            title = "Wayland startup script",
                            initialText = AppPrefs.readDebianWaylandStartupScript(prefs),
                            onCancel = {
                                onDebianWaylandScriptEditorOpenChange(false)
                            },
                            onSave = {
                                AppPrefs.writeDebianWaylandStartupScript(prefs, it)
                                onDebianWaylandScriptEditorOpenChange(false)
                            },
                        )
                    }
                    "x11-script" -> {
                        DrawerScriptEditor(
                            title = "X11 startup script",
                            initialText = AppPrefs.readDebianX11StartupScript(prefs),
                            onCancel = {
                                onDebianX11ScriptEditorOpenChange(false)
                            },
                            onSave = {
                                AppPrefs.writeDebianX11StartupScript(prefs, it)
                                onDebianX11ScriptEditorOpenChange(false)
                            },
                        )
                    }
                    else -> {
                        DrawerProtocolRow(
                            protocol = displayProtocol,
                            onProtocolSelect = {
                                displayProtocol = it
                                closeScriptEditors()
                            },
                            onConfigureScript = {
                                closeScriptEditors()
                                when (displayProtocol) {
                                    DisplayProtocol.WAYLAND ->
                                        onDebianWaylandScriptEditorOpenChange(true)
                                    DisplayProtocol.X11 ->
                                        onDebianX11ScriptEditorOpenChange(true)
                                }
                            },
                        )
                    }
                }
            }
        },
        desktopViewContent = {
            when (displayProtocol) {
                DisplayProtocol.WAYLAND -> {
                    DrawerWaylandViewSettings(
                        mouseModeLabel = waylandMouseModeLabel,
                        onMouseModeSelectLabel = onWaylandMouseModeSelectLabel,
                        resolutionPercentLabel = waylandResolutionLabel,
                        onResolutionPercentSelectLabel = onWaylandResolutionPercentSelectLabel,
                        scalePercentLabel = waylandScaleLabel,
                        onScalePercentSelectLabel = onWaylandScalePercentSelectLabel,
                    )
                }
                DisplayProtocol.X11 -> {
                    DrawerEmbeddedX11ViewSettings(
                        x11MouseModeLabel = x11MouseModeLabel,
                        onX11MouseModeSelectLabel = onX11MouseModeSelectLabel,
                        x11ResolutionModeLabel = x11ResolutionModeLabel,
                        onX11ResolutionModeSelectLabel = onX11ResolutionModeSelectLabel,
                        x11DisplayScaleLabel = x11DisplayScaleLabel,
                        onX11DisplayScaleSelectLabel = onX11DisplayScaleSelectLabel,
                        x11ResolutionExactLabel = x11ResolutionExactLabel,
                        onX11ResolutionExactSelectLabel = onX11ResolutionExactSelectLabel,
                        x11ResolutionCustom = x11ResolutionCustom,
                        onX11ResolutionCustomChange = onX11ResolutionCustomChange,
                        onX11ResolutionCustomApply = onX11ResolutionCustomApply,
                    )
                }
            }
        },
        footerContent = {
            if (!scriptEditorOpen) {
                DrawerDesktopLaunchButton(
                    onClick = {
                        scope.launch { drawerState.close() }
                        when (displayProtocol) {
                            DisplayProtocol.WAYLAND -> onEnterDebianWayland()
                            DisplayProtocol.X11 -> onEnterDebianX11()
                        }
                    },
                )
            }
        },
    )
}
