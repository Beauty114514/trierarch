package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
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
import app.trierarch.ui.drawer.menu.DrawerDropdownField
import app.trierarch.ui.drawer.menu.DrawerExpandableSection
import app.trierarch.ui.prefs.ColdStartConfig
import app.trierarch.ui.prefs.RootfsPlatform
import app.trierarch.ui.prefs.RootfsSessionMode
import com.winlator.container.ContainerManager

@Composable
fun TrierarchDrawerPage(
    coldStartConfig: ColdStartConfig,
    onColdStartConfigChange: (ColdStartConfig) -> Unit,
    onShowKeyboard: () -> Unit,
) {
    val context = LocalContext.current
    var containerNamesById by remember { mutableStateOf<Map<Int, String>>(emptyMap()) }

    LaunchedEffect(Unit) {
        val manager = ContainerManager(context)
        containerNamesById = manager.getContainers().associate { it.id to it.name }
    }

    val platformLabel = coldStartConfig.platform.label
    val modeLabel = coldStartConfig.modeLabel(containerNamesById)
    val modeOptions = remember(coldStartConfig.platform, containerNamesById) {
        when (coldStartConfig.platform) {
            RootfsPlatform.WINE -> buildList {
                add(RootfsSessionMode.WINE_CONTAINER_PICKER.label)
                containerNamesById.entries
                    .sortedBy { it.value.lowercase() }
                    .forEach { (_, name) -> add(name) }
            }
            RootfsPlatform.ARCH, RootfsPlatform.DEBIAN ->
                RootfsSessionMode.ARCH_DEBIAN.map { it.label }
        }
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
    ) {
        Text(
            text = "Trierarch",
            style = MaterialTheme.typography.titleLarge,
            color = drawerPageAccent(),
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
        )

        DrawerExpandableSection(title = "Default startup", defaultExpanded = true) {
            DrawerDropdownField(
                label = "Environment",
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
                        RootfsPlatform.WINE -> when (label) {
                            RootfsSessionMode.WINE_CONTAINER_PICKER.label -> ColdStartConfig.wineContainerPicker()
                            else -> {
                                val id = containerNamesById.entries.find { it.value == label }?.key
                                if (id != null) ColdStartConfig.wineContainer(id) else coldStartConfig
                            }
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
        Text(
            text = "Keyboard",
            style = MaterialTheme.typography.bodyLarge,
            color = drawerPageAccent(),
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onShowKeyboard() }
                .padding(vertical = 12.dp, horizontal = 12.dp),
        )

        Spacer(Modifier.height(8.dp))
    }
}
