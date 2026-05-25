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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

@Composable
fun ArchDrawerPage(
    prefs: SharedPreferences,
    drawerState: DrawerState,
    scope: CoroutineScope,
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
    archWaylandScriptEditorOpen: Boolean,
    onArchWaylandScriptEditorOpenChange: (Boolean) -> Unit,
    archX11ScriptEditorOpen: Boolean,
    onArchX11ScriptEditorOpenChange: (Boolean) -> Unit,
    onEnterArchWayland: () -> Unit,
    onEnterArchX11: () -> Unit,
    onEnterTerminal: () -> Unit,
    onDesktopVulkanSelect: (String) -> Unit,
    onDesktopOpenGLSelect: (String) -> Unit,
    onWaylandMouseModeSelectLabel: (String) -> Unit,
    onWaylandResolutionPercentSelectLabel: (String) -> Unit,
    onWaylandScalePercentSelectLabel: (String) -> Unit,
    vulkanOptions: List<String>,
    openGLOptions: List<String>,
) {
    var displayProtocol by remember { mutableStateOf(DisplayProtocol.WAYLAND) }
    val scriptEditorOpen = archWaylandScriptEditorOpen || archX11ScriptEditorOpen
    val waylandMouseModeLabel =
        if (mouseMode == MOUSE_MODE_TABLET) "Tablet" else "Touchpad"
    val waylandResolutionLabel = "${resolutionPercent.coerceIn(10, 100)}%"
    val waylandScaleLabel = "${scalePercent.coerceIn(100, 1000)}%"

    fun closeScriptEditors() {
        onArchWaylandScriptEditorOpenChange(false)
        onArchX11ScriptEditorOpenChange(false)
    }

    DrawerMenu(
        title = "Arch",
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
                onEnterTerminal()
            },
            onViewClick = { scope.launch { drawerState.close() } },
            onAppearanceClick = { scope.launch { drawerState.close() } },
            onSessionClick = { scope.launch { drawerState.close() } },
            onKeyboardClick = { scope.launch { drawerState.close() } },
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
                archWaylandScriptEditorOpen -> "wayland-script"
                archX11ScriptEditorOpen -> "x11-script"
                else -> "protocol"
            }
            AnimatedContent(
                targetState = headerMode,
                transitionSpec = {
                    (fadeIn() + expandVertically()).togetherWith(fadeOut() + shrinkVertically())
                },
                label = "Arch protocol script editor",
            ) { mode ->
                when (mode) {
                    "wayland-script" -> {
                        DrawerScriptEditor(
                            title = "Wayland startup script",
                            initialText = AppPrefs.readArchWaylandStartupScript(prefs),
                            onCancel = {
                                onArchWaylandScriptEditorOpenChange(false)
                            },
                            onSave = {
                                AppPrefs.writeArchWaylandStartupScript(prefs, it)
                                onArchWaylandScriptEditorOpenChange(false)
                            },
                        )
                    }
                    "x11-script" -> {
                        DrawerScriptEditor(
                            title = "X11 startup script",
                            initialText = AppPrefs.readArchX11StartupScript(prefs),
                            onCancel = {
                                onArchX11ScriptEditorOpenChange(false)
                            },
                            onSave = {
                                AppPrefs.writeArchX11StartupScript(prefs, it)
                                onArchX11ScriptEditorOpenChange(false)
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
                                    DisplayProtocol.WAYLAND -> onArchWaylandScriptEditorOpenChange(true)
                                    DisplayProtocol.X11 -> onArchX11ScriptEditorOpenChange(true)
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
                            DisplayProtocol.WAYLAND -> onEnterArchWayland()
                            DisplayProtocol.X11 -> onEnterArchX11()
                        }
                    },
                )
            }
        },
    )
}
