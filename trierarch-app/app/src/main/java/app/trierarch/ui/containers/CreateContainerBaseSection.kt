@file:OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)

package app.trierarch.ui.containers

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.winlator.container.AudioDrivers
import com.winlator.container.DXWrappers
import com.winlator.container.GraphicsDrivers

@Composable
internal fun CreateContainerBaseSection(
    name: String,
    onNameChange: (String) -> Unit,
    screenSize: String,
    onScreenSizeChange: (String) -> Unit,
    screenSizeMenuOpen: Boolean,
    onScreenSizeMenuOpenChange: (Boolean) -> Unit,
    useCustomScreenSize: Boolean,
    onUseCustomScreenSizeChange: (Boolean) -> Unit,
    customScreenWidth: String,
    onCustomScreenWidthChange: (String) -> Unit,
    customScreenHeight: String,
    onCustomScreenHeightChange: (String) -> Unit,
    hudMode: Int,
    onHudModeChange: (Int) -> Unit,
    hudModeMenuOpen: Boolean,
    onHudModeMenuOpenChange: (Boolean) -> Unit,
    vulkanDriver: String,
    onVulkanDriverChange: (String) -> Unit,
    openGLDriver: String,
    onOpenGLDriverChange: (String) -> Unit,
    onOpenGraphicsConfig: (String) -> Unit,
    direct3DWrapper: String,
    onDirect3DWrapperChange: (String) -> Unit,
    onOpenDxWrapperConfig: (String) -> Unit,
    audioDriver: String,
    onAudioDriverChange: (String) -> Unit,
    onOpenAudioConfig: () -> Unit,
) {
    fun parseIdentifier(value: String): String {
        // Upstream: StringUtils.parseIdentifier("640x360 (16:9)") -> "640x360"
        return value.substringBefore(" ").trim()
    }

    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        OutlinedTextField(
            value = name,
            onValueChange = onNameChange,
            label = { Text("Name") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )

        Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f)) {
                Text("Screen size", style = MaterialTheme.typography.labelMedium)
                Spacer(Modifier.height(6.dp))
                val screenSizeEntries = remember {
                    listOf(
                        "Custom",
                        "640x360 (16:9)",
                        "640x480 (4:3)",
                        "800x600 (4:3)",
                        "854x480 (16:9)",
                        "960x544 (16:9)",
                        "1024x768 (4:3)",
                        "1280x720 (16:9)",
                        "1280x800 (16:10)",
                        "1280x1024 (5:4)",
                        "1366x768 (16:9)",
                        "1440x900 (16:10)",
                        "1600x900 (16:9)",
                        "1920x1080 (16:9)",
                    )
                }
                val selectedLabel = remember(screenSize, useCustomScreenSize) {
                    if (useCustomScreenSize) {
                        "Custom"
                    } else {
                        screenSizeEntries.firstOrNull { parseIdentifier(it) == screenSize } ?: screenSize
                    }
                }

                TextButton(onClick = { onScreenSizeMenuOpenChange(true) }) { Text(selectedLabel) }
                DropdownMenu(expanded = screenSizeMenuOpen, onDismissRequest = { onScreenSizeMenuOpenChange(false) }) {
                    screenSizeEntries.forEach { v ->
                        DropdownMenuItem(
                            text = { Text(v) },
                            onClick = {
                                if (v == "Custom") {
                                    onUseCustomScreenSizeChange(true)
                                } else {
                                    onScreenSizeChange(parseIdentifier(v))
                                    onUseCustomScreenSizeChange(false)
                                }
                                onScreenSizeMenuOpenChange(false)
                            },
                        )
                    }
                }
            }
            Column(modifier = Modifier.weight(1f)) {
                Text("HUD", style = MaterialTheme.typography.labelMedium)
                Spacer(Modifier.height(6.dp))
                TextButton(onClick = { onHudModeMenuOpenChange(true) }) {
                    Text(
                        when (hudMode) {
                            1 -> "Simple"
                            2 -> "Full"
                            else -> "Disabled"
                        }
                    )
                }
                DropdownMenu(expanded = hudModeMenuOpen, onDismissRequest = { onHudModeMenuOpenChange(false) }) {
                    listOf(0 to "Disabled", 1 to "Simple", 2 to "Full").forEach { (id, label) ->
                        DropdownMenuItem(
                            text = { Text(label) },
                            onClick = {
                                onHudModeChange(id)
                                onHudModeMenuOpenChange(false)
                            },
                        )
                    }
                }
            }
        }

        if (useCustomScreenSize) {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                OutlinedTextField(
                    value = customScreenWidth,
                    onValueChange = onCustomScreenWidthChange,
                    label = { Text("Width (even)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    singleLine = true,
                    modifier = Modifier.weight(1f),
                )
                OutlinedTextField(
                    value = customScreenHeight,
                    onValueChange = onCustomScreenHeightChange,
                    label = { Text("Height (even)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    singleLine = true,
                    modifier = Modifier.weight(1f),
                )
            }
        }

        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Graphics driver", style = MaterialTheme.typography.labelMedium)
            val vulkanOptions = remember {
                listOf(
                    PickerOption(GraphicsDrivers.TURNIP, GraphicsDrivers.getName(GraphicsDrivers.TURNIP)),
                    PickerOption(GraphicsDrivers.VORTEK, GraphicsDrivers.getName(GraphicsDrivers.VORTEK)),
                )
            }
            val openGLOptions = remember {
                listOf(
                    PickerOption(GraphicsDrivers.ZINK, GraphicsDrivers.getName(GraphicsDrivers.ZINK)),
                    PickerOption(GraphicsDrivers.VIRGL, GraphicsDrivers.getName(GraphicsDrivers.VIRGL)),
                    PickerOption(GraphicsDrivers.GLADIO, GraphicsDrivers.getName(GraphicsDrivers.GLADIO)),
                )
            }

            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                SelectionBoxWithConfig(
                    label = "Vulkan",
                    selectedId = vulkanDriver,
                    options = vulkanOptions,
                    onSelect = onVulkanDriverChange,
                    onConfig = { onOpenGraphicsConfig("Vulkan") },
                    modifier = Modifier.weight(1f),
                )
                SelectionBoxWithConfig(
                    label = "OpenGL",
                    selectedId = openGLDriver,
                    options = openGLOptions,
                    onSelect = onOpenGLDriverChange,
                    onConfig = { onOpenGraphicsConfig("OpenGL") },
                    configEnabled = openGLDriver != GraphicsDrivers.ZINK && openGLDriver != GraphicsDrivers.GLADIO,
                    modifier = Modifier.weight(1f),
                )
            }
        }

        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("DX Wrapper", style = MaterialTheme.typography.labelMedium)
            val d3dOptions = remember {
                listOf(
                    PickerOption(DXWrappers.WINED3D, DXWrappers.getName(DXWrappers.WINED3D)),
                    PickerOption(DXWrappers.DXVK, DXWrappers.getName(DXWrappers.DXVK)),
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                SelectionBoxWithConfig(
                    label = "Direct3D",
                    selectedId = direct3DWrapper,
                    options = d3dOptions,
                    onSelect = onDirect3DWrapperChange,
                    onConfig = { onOpenDxWrapperConfig("Direct3D") },
                    modifier = Modifier.weight(1f),
                )
                SelectionBoxWithConfig(
                    label = "DirectX 12",
                    selectedId = DXWrappers.VKD3D,
                    options = listOf(PickerOption(DXWrappers.VKD3D, DXWrappers.getName(DXWrappers.VKD3D))),
                    onSelect = { /* fixed: VKD3D only */ },
                    onConfig = { onOpenDxWrapperConfig("DirectX 12") },
                    enabled = true,
                    modifier = Modifier.weight(1f),
                )
            }
        }

        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Audio driver", style = MaterialTheme.typography.labelMedium)
            SelectionBoxWithConfig(
                label = "Driver",
                selectedId = audioDriver,
                options = listOf(
                    PickerOption(AudioDrivers.ALSA, "ALSA"),
                    PickerOption(AudioDrivers.PULSEAUDIO, "PulseAudio"),
                ),
                onSelect = onAudioDriverChange,
                onConfig = onOpenAudioConfig,
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

