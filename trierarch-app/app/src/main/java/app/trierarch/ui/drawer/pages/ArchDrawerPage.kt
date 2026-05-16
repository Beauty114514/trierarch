package app.trierarch.ui.drawer.pages

import android.content.SharedPreferences
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.material3.DrawerState
import app.trierarch.TerminalSessionIds
import app.trierarch.shell.ShellFonts
import app.trierarch.ui.dialog.MOUSE_MODE_TABLET
import app.trierarch.ui.drawer.menu.DrawerExpandableSection
import app.trierarch.ui.drawer.menu.DrawerMenu
import app.trierarch.ui.drawer.menu.DrawerMenuActions
import app.trierarch.ui.drawer.menu.DrawerMenuLabels
import app.trierarch.ui.drawer.menu.DrawerMenuOptions
import app.trierarch.ui.drawer.menu.DrawerMenuVisibility
import app.trierarch.ui.drawer.menu.DrawerScriptEditor
import app.trierarch.ui.prefs.AppPrefs
import app.trierarch.ui.runtime.TerminalSessionController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

@Composable
fun ArchDrawerPage(
    prefs: SharedPreferences,
    drawerState: DrawerState,
    scope: CoroutineScope,
    terminalFontKey: String,
    terminalSessionState: TerminalSessionController.State,
    desktopVulkanMode: String,
    desktopOpenGLMode: String,
    mouseMode: Int,
    resolutionPercent: Int,
    scalePercent: Int,
    archWaylandScriptEditorOpen: Boolean,
    onArchWaylandScriptEditorOpenChange: (Boolean) -> Unit,
    archX11ScriptEditorOpen: Boolean,
    onArchX11ScriptEditorOpenChange: (Boolean) -> Unit,
    onEnterArchWayland: () -> Unit,
    onEnterArchX11: () -> Unit,
    onEnterTerminal: () -> Unit,
    onDesktopVulkanSelect: (String) -> Unit,
    onDesktopOpenGLSelect: (String) -> Unit,
    onTerminalFontSelectLabel: (String) -> Unit,
    onTerminalSessionStateChange: (TerminalSessionController.State) -> Unit,
    onMouseModeSelectLabel: (String) -> Unit,
    onResolutionPercentSelectLabel: (String) -> Unit,
    onScalePercentSelectLabel: (String) -> Unit,
    vulkanOptions: List<String>,
    openGLOptions: List<String>,
) {
    val terminalFontLabel = remember(terminalFontKey) {
        ShellFonts.options.find { it.id == terminalFontKey }?.label
            ?: ShellFonts.options.firstOrNull()?.label
            ?: terminalFontKey
    }
    val terminalSessionLabel = remember(terminalSessionState.activeSessionId) {
        TerminalSessionIds.sessionPickerLine(terminalSessionState.activeSessionId)
    }
    val mouseModeLabel = remember(mouseMode) {
        if (mouseMode == MOUSE_MODE_TABLET) "Tablet" else "Touchpad"
    }
    val resolutionLabel = remember(resolutionPercent) { "${resolutionPercent.coerceIn(10, 100)}%" }
    val scaleLabel = remember(scalePercent) { "${scalePercent.coerceIn(100, 1000)}%" }

    DrawerMenu(
        title = "Arch",
        labels = DrawerMenuLabels(
            launcherDefaultLabel = "",
            desktopVulkanLabel = desktopVulkanMode,
            desktopOpenGLLabel = desktopOpenGLMode,
            terminalFontLabel = terminalFontLabel,
            terminalSessionLabel = terminalSessionLabel,
            mouseModeLabel = mouseModeLabel,
            resolutionPercentLabel = resolutionLabel,
            scalePercentLabel = scaleLabel,
        ),
        options = DrawerMenuOptions(
            launcherDefaultOptions = emptyList(),
            desktopVulkanOptions = vulkanOptions,
            desktopOpenGLOptions = openGLOptions,
            terminalFontOptions = ShellFonts.options.map { it.label },
            terminalSessionOptions = buildList {
                add(TerminalSessionIds.sessionPickerLine(TerminalSessionIds.FIRST_TERMINAL))
                terminalSessionState.sessionIds
                    .sorted()
                    .filter { it != TerminalSessionIds.FIRST_TERMINAL }
                    .forEach { add(TerminalSessionIds.sessionPickerLine(it)) }
                add("New session")
                add("Close current session")
            },
            mouseModeOptions = listOf("Touchpad", "Tablet"),
            resolutionPercentOptions = (10..100 step 10).map { "${it}%" },
            scalePercentOptions = (100..1000 step 100).map { "${it}%" },
        ),
        actions = DrawerMenuActions(
            onWaylandEntryClick = {
                scope.launch { drawerState.close() }
                onEnterArchWayland()
            },
            onWaylandEntryLongPress = { onArchWaylandScriptEditorOpenChange(true) },
            onX11EntryClick = {
                scope.launch { drawerState.close() }
                onEnterArchX11()
            },
            onX11EntryLongPress = { onArchX11ScriptEditorOpenChange(true) },
            onTerminalClick = {
                scope.launch { drawerState.close() }
                onTerminalSessionStateChange(terminalSessionState.copy(activeSessionId = TerminalSessionIds.ARCH_TERMINAL))
                onEnterTerminal()
            },
            onViewClick = { scope.launch { drawerState.close() } },
            onAppearanceClick = { scope.launch { drawerState.close() } },
            onSessionClick = { scope.launch { drawerState.close() } },
            onKeyboardClick = { scope.launch { drawerState.close() } },
            onLauncherDefaultSelect = { },
            onDesktopVulkanSelect = onDesktopVulkanSelect,
            onDesktopOpenGLSelect = onDesktopOpenGLSelect,
            onTerminalFontSelect = onTerminalFontSelectLabel,
            onTerminalSessionSelect = { label ->
                val next = when (label) {
                    "New session" ->
                        TerminalSessionController.addNewInteractiveSession(
                            terminalSessionState,
                            TerminalSessionIds.RootfsRow.ARCH,
                        )
                    "Close current session" ->
                        TerminalSessionController.closeCurrentSession(terminalSessionState)
                    else ->
                        TerminalSessionController.selectFromPickerLine(terminalSessionState, label)
                }
                onTerminalSessionStateChange(next)
            },
            onMouseModeSelect = onMouseModeSelectLabel,
            onResolutionPercentSelect = onResolutionPercentSelectLabel,
            onScalePercentSelect = onScalePercentSelectLabel,
            onCloseDrawerRequest = { scope.launch { drawerState.close() } },
        ),
        visibility = DrawerMenuVisibility(
            launcherDefault = false,
            graphics = true,
            terminalSettings = true,
            showWaylandEntry = true,
            showX11Entry = true,
            terminalEntry = true,
            desktopView = true,
            keyboard = true,
        ),
        extraContent = {
            DrawerExpandableSection(title = "Scripts", defaultExpanded = false) {
                if (archWaylandScriptEditorOpen) {
                    DrawerScriptEditor(
                        title = "Wayland startup script",
                        initialText = AppPrefs.readArchWaylandStartupScript(prefs),
                        onSave = {
                            AppPrefs.writeArchWaylandStartupScript(prefs, it)
                            onArchWaylandScriptEditorOpenChange(false)
                        },
                    )
                } else if (archX11ScriptEditorOpen) {
                    DrawerScriptEditor(
                        title = "X11 startup script",
                        initialText = AppPrefs.readArchX11StartupScript(prefs),
                        onSave = {
                            AppPrefs.writeArchX11StartupScript(prefs, it)
                            onArchX11ScriptEditorOpenChange(false)
                        },
                    )
                } else {
                    Text(
                        text = "Edit Wayland startup script",
                        style = MaterialTheme.typography.bodyLarge,
                        color = drawerPageAccent(),
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { onArchWaylandScriptEditorOpenChange(true) }
                            .padding(vertical = 12.dp, horizontal = 12.dp),
                    )
                    Text(
                        text = "Edit X11 startup script",
                        style = MaterialTheme.typography.bodyLarge,
                        color = drawerPageAccent(),
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { onArchX11ScriptEditorOpenChange(true) }
                            .padding(vertical = 12.dp, horizontal = 12.dp),
                    )
                }
            }
        },
    )
}
