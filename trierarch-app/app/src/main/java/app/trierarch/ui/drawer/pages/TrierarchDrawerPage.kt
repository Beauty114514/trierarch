package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import app.trierarch.TerminalSessionIds
import app.trierarch.shell.ShellFonts
import app.trierarch.ui.drawer.menu.DrawerDropdownField
import app.trierarch.ui.drawer.menu.DrawerExpandableSection
import app.trierarch.ui.drawer.menu.DrawerPageHeader
import app.trierarch.ui.drawer.menu.drawerEndPadding
import app.trierarch.ui.drawer.menu.drawerRowTextStyle
import app.trierarch.ui.drawer.menu.drawerRowVerticalPadding
import app.trierarch.ui.drawer.menu.drawerStartPadding
import app.trierarch.ui.prefs.ColdStartConfig
import app.trierarch.ui.runtime.TerminalSessionController
import app.trierarch.ui.prefs.RootfsPlatform
import app.trierarch.ui.prefs.RootfsSessionMode
import com.winlator.container.ContainerManager

@Composable
fun TrierarchDrawerPage(
    coldStartConfig: ColdStartConfig,
    onColdStartConfigChange: (ColdStartConfig) -> Unit,
    terminalFontKey: String,
    onTerminalFontSelectLabel: (String) -> Unit,
    terminalSessionState: TerminalSessionController.State,
    terminalMgmtRootfs: TerminalSessionIds.RootfsRow,
    onTerminalMgmtRootfsChange: (TerminalSessionIds.RootfsRow) -> Unit,
    onTerminalSessionStateChange: (TerminalSessionController.State) -> Unit,
    onShowKeyboard: () -> Unit,
    onCloseDrawer: () -> Unit,
) {
    val terminalFontLabel = remember(terminalFontKey) {
        ShellFonts.options.find { it.id == terminalFontKey }?.label
            ?: ShellFonts.options.firstOrNull()?.label
            ?: terminalFontKey
    }
    val terminalMgmtLabel = remember(terminalMgmtRootfs) {
        when (terminalMgmtRootfs) {
            TerminalSessionIds.RootfsRow.ARCH -> RootfsPlatform.ARCH.label
            TerminalSessionIds.RootfsRow.DEBIAN -> RootfsPlatform.DEBIAN.label
        }
    }
    val terminalSessionLabel = remember(terminalSessionState, terminalMgmtRootfs) {
        TerminalSessionIds.sessionPickerLine(
            TerminalSessionController.displaySessionId(terminalSessionState, terminalMgmtRootfs),
        )
    }
    val terminalSessionOptions = remember(terminalSessionState, terminalMgmtRootfs) {
        TerminalSessionController.sessionPickerOptions(terminalSessionState, terminalMgmtRootfs)
    }
    val context = LocalContext.current
    var containerNamesById by remember { mutableStateOf<Map<Int, String>>(emptyMap()) }

    LaunchedEffect(Unit) {
        val manager = ContainerManager(context)
        containerNamesById = manager.getContainers().associate { it.id to it.name }
    }

    val wineContainerOptions = remember(containerNamesById) {
        containerNamesById.entries
            .sortedBy { it.value.lowercase() }
            .map { it.key to it.value }
    }

    val platformLabel = coldStartConfig.platform.label
    val modeLabel = when (coldStartConfig.platform) {
        RootfsPlatform.WINE ->
            coldStartConfig.selectedWineContainerId?.let { id ->
                containerNamesById[id] ?: "Container $id"
            } ?: if (wineContainerOptions.isEmpty()) "No containers" else "Select container"
        RootfsPlatform.ARCH, RootfsPlatform.DEBIAN -> coldStartConfig.modeLabel(containerNamesById)
    }
    val modeOptions = remember(coldStartConfig.platform, containerNamesById) {
        when (coldStartConfig.platform) {
            RootfsPlatform.WINE -> wineContainerOptions.map { it.second }
            RootfsPlatform.ARCH, RootfsPlatform.DEBIAN ->
                RootfsSessionMode.ARCH_DEBIAN.map { it.label }
        }
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
    ) {
        DrawerPageHeader(
            title = "Trierarch",
            onClose = onCloseDrawer,
        )

        DrawerExpandableSection(title = "Default startup", defaultExpanded = true) {
            DrawerDropdownField(
                label = "Platform",
                value = platformLabel,
                options = RootfsPlatform.entries.map { it.label },
                onSelect = { label ->
                    val platform = RootfsPlatform.entries.find { it.label == label } ?: RootfsPlatform.ARCH
                    val sessionTarget = when (platform) {
                        RootfsPlatform.WINE -> RootfsSessionMode.WINE_CONTAINER_PICKER.prefValue
                        else -> RootfsSessionMode.TERMINAL.prefValue
                    }
                    onColdStartConfigChange(ColdStartConfig(platform, sessionTarget))
                },
            )
            DrawerDropdownField(
                label = "Mode",
                value = modeLabel,
                options = modeOptions,
                onSelect = { label ->
                    val next = when (coldStartConfig.platform) {
                        RootfsPlatform.WINE -> {
                            val id = wineContainerOptions.find { it.second == label }?.first
                            if (id != null) ColdStartConfig.wineContainer(id) else coldStartConfig
                        }
                        RootfsPlatform.ARCH -> {
                            val mode = RootfsSessionMode.ARCH_DEBIAN.find { it.label == label }
                                ?: RootfsSessionMode.TERMINAL
                            ColdStartConfig.withRootfsSession(RootfsPlatform.ARCH, mode)
                        }
                        RootfsPlatform.DEBIAN -> {
                            val mode = RootfsSessionMode.ARCH_DEBIAN.find { it.label == label }
                                ?: RootfsSessionMode.TERMINAL
                            ColdStartConfig.withRootfsSession(RootfsPlatform.DEBIAN, mode)
                        }
                    }
                    onColdStartConfigChange(next)
                },
            )
        }

        Spacer(Modifier.height(6.dp))
        DrawerExpandableSection(title = "Terminal", defaultExpanded = false) {
            DrawerDropdownField(
                label = "Distro",
                value = terminalMgmtLabel,
                options = listOf(RootfsPlatform.ARCH.label, RootfsPlatform.DEBIAN.label),
                onSelect = { label ->
                    val row = when (label) {
                        RootfsPlatform.DEBIAN.label -> TerminalSessionIds.RootfsRow.DEBIAN
                        else -> TerminalSessionIds.RootfsRow.ARCH
                    }
                    onTerminalMgmtRootfsChange(row)
                },
            )
            DrawerDropdownField(
                label = "Session",
                value = terminalSessionLabel,
                options = terminalSessionOptions,
                onSelect = { label ->
                    onTerminalSessionStateChange(
                        TerminalSessionController.handleSessionPickerSelect(
                            terminalSessionState,
                            terminalMgmtRootfs,
                            label,
                        ),
                    )
                },
            )
            DrawerDropdownField(
                label = "Appearance",
                value = terminalFontLabel,
                options = ShellFonts.options.map { it.label },
                onSelect = onTerminalFontSelectLabel,
            )
        }

        Spacer(Modifier.height(6.dp))
        Text(
            text = "Keyboard",
            style = drawerRowTextStyle(),
            color = drawerPageAccent(),
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onShowKeyboard() }
                .padding(
                    start = drawerStartPadding(),
                    end = drawerEndPadding(),
                    top = drawerRowVerticalPadding(),
                    bottom = drawerRowVerticalPadding(),
                ),
        )

        Spacer(Modifier.height(8.dp))
    }
}
