@file:OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)

package app.trierarch.ui.containers

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.winlator.container.DXWrappers
import com.winlator.core.KeyValueSet
import com.winlator.core.StringUtils
import com.winlator.core.WineInfo
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

/**
 * Matches shipped archives under `assets/graphics_driver` (`.tzst` bundles; one artifact per family).
 * Used for read-only labels only — no extra installable components or download UI.
 */
internal object BundledGraphicsDriverVersions {
    const val TURNIP = "26.1.0"
    const val VORTEK = "2.1"
    const val VIRGL = "23.1.9"
}

/** Shipped `dxwrapper` archives (e.g. vkd3d-2.14.1.tzst on disk). */
internal object BundledDxWrapperArtifacts {
    const val VKD3D = "2.14.1"
}

@Composable
internal fun SelectionBoxWithConfig(
    label: String,
    selectedId: String,
    options: List<PickerOption>,
    onSelect: (String) -> Unit,
    onConfig: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    /** When false, the settings gear is disabled but the selection dropdown still follows [enabled]. */
    configEnabled: Boolean = true,
) {
    var menuOpen by remember { mutableStateOf(false) }
    Column(modifier = modifier) {
        Text(label, style = MaterialTheme.typography.labelMedium)
        Spacer(Modifier.height(6.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            Box(modifier = Modifier.weight(1f)) {
                val selectedLabel = options.firstOrNull { it.id == selectedId }?.label ?: selectedId
                TextButton(
                    onClick = { if (enabled) menuOpen = true },
                    enabled = enabled,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(selectedLabel, maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
                DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
                    options.forEach { opt ->
                        DropdownMenuItem(
                            text = { Text(opt.label) },
                            onClick = {
                                onSelect(opt.id)
                                menuOpen = false
                            },
                        )
                    }
                }
            }
            IconButton(onClick = onConfig, enabled = enabled && configEnabled) {
                Icon(Icons.Filled.Settings, contentDescription = "Config")
            }
        }
    }
}

@Composable
internal fun RawConfigDialog(
    title: String,
    value: String,
    onValueChange: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = {
            OutlinedTextField(
                value = value,
                onValueChange = onValueChange,
                label = { Text("Config (raw)") },
                modifier = Modifier.fillMaxWidth(),
            )
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("OK") } },
    )
}

@Composable
internal fun AudioConfigDialog(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }
    // Upstream: AudioDriverConfigDialog.DEFAULT_PERFORMANCE_MODE = 1 (Low latency)
    var performanceMode by remember { mutableStateOf(config.getInt("performanceMode", 1).coerceIn(0, 2)) }
    var volumePercent by remember { mutableStateOf((config.getFloat("volume", 1.0f) * 100f).coerceIn(0f, 100f)) }
    // Upstream: min 12, max 120, step 2, default 16.
    var latencyMillis by remember { mutableStateOf(config.getInt("latencyMillis", 16).coerceIn(12, 120)) }

    var perfMenuOpen by remember { mutableStateOf(false) }
    val perfOptions = listOf(
        0 to "None",
        1 to "Low latency",
        2 to "Power saving",
    )

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Audio configuration") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                Text("Performance Mode", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { perfMenuOpen = true }) {
                        Text(perfOptions.firstOrNull { it.first == performanceMode }?.second ?: performanceMode.toString())
                    }
                    DropdownMenu(expanded = perfMenuOpen, onDismissRequest = { perfMenuOpen = false }) {
                        perfOptions.forEach { (id, label) ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    performanceMode = id
                                    perfMenuOpen = false
                                },
                            )
                        }
                    }
                }

                Text("Volume (${volumePercent.toInt()}%)", style = MaterialTheme.typography.labelMedium)
                Slider(
                    value = volumePercent,
                    onValueChange = { volumePercent = it },
                    valueRange = 0f..100f,
                    steps = 99,
                )

                Text("Average Latency (${latencyMillis} ms)", style = MaterialTheme.typography.labelMedium)
                Slider(
                    value = latencyMillis.toFloat(),
                    onValueChange = { v ->
                        val stepped = (v / 2f).toInt() * 2
                        latencyMillis = stepped.coerceIn(12, 120)
                    },
                    valueRange = 12f..120f,
                    steps = 53,
                )
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val newConfig = KeyValueSet()
                    newConfig.put("performanceMode", performanceMode)
                    newConfig.put("volume", volumePercent / 100.0f)
                    newConfig.put("latencyMillis", latencyMillis)
                    onConfirm(newConfig.toString())
                }
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

private fun maxDeviceMemoryEntries(): List<String> = listOf(
    "0 (No limit)",
    "512 MB",
    "1 GB",
    "2 GB",
    "4 GB",
    "6 GB",
    "8 GB",
    "12 GB",
)

/** WineD3D `VideoMemorySize` spinner labels (aligned with upstream Winlator `video_memory_size_entries`). */
private fun videoMemorySizeEntries(): List<String> = listOf(
    "32 MB",
    "64 MB",
    "128 MB",
    "256 MB",
    "512 MB",
    "1 GB",
    "2 GB",
    "4 GB",
    "6 GB",
    "8 GB",
    "12 GB",
)

private fun dxvkFramerateEntries(): List<String> = listOf(
    "0 (No limit)",
    "30",
    "60",
    "120",
)

private fun ddrawWrapperEntries(): List<Pair<String, String>> = listOf(
    DXWrappers.WINED3D to "WineD3D",
    DXWrappers.CNC_DDRAW to "CNC DDraw",
)

private fun wined3dRendererEntries(): List<Pair<String, String>> = listOf(
    "gdi" to "GDI",
    "gl" to "GL",
    "vulkan" to "Vulkan",
)

private fun offscreenRenderingModeEntries(): List<Pair<String, String>> = listOf(
    "backbuffer" to "Backbuffer",
    "fbo" to "FBO",
)

/** Upstream Winlator `d3d_feature_level_entries` (guest D3D feature level cap for VKD3D). */
private fun d3dFeatureLevelEntries(): List<String> = listOf(
    "11.0",
    "11.1",
    "12.0",
    "12.1",
    "12.2",
)

private fun parseMemorySizeLabelToValue(label: String): String {
    val trimmed = label.trim()
    if (trimmed.startsWith("0")) return "0"
    return StringUtils.parseMemorySize(trimmed)
}

private fun labelForMemoryConfig(entries: List<String>, storedMb: String): String {
    val v = storedMb.trim()
    if (v.isEmpty() || v == "0") return entries.first()
    entries.forEach { label ->
        if (label.startsWith("0")) return@forEach
        if (StringUtils.parseMemorySize(label) == v) return label
    }
    return formatMemorySizeValueToFallbackLabel(v)
}

private fun formatMemorySizeValueToFallbackLabel(mbValue: String): String {
    val n = mbValue.toLongOrNull() ?: return mbValue
    if (n <= 0L) return "0 (No limit)"
    if (n % 1024L == 0L) return "${n / 1024L} GB"
    return "$n MB"
}

private fun formatMemorySizeValueToLabel(value: String): String {
    val v = value.trim()
    if (v.isEmpty() || v == "0") return "0 (No limit)"
    return formatMemorySizeValueToFallbackLabel(v.removeSuffix("MB").removeSuffix("mb").trim())
}

@Composable
internal fun TurnipConfigDialogLite(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
    fixedVersion: String = BundledGraphicsDriverVersions.TURNIP,
) {
    // Upstream keys: version, maxDeviceMemory, useHWBuf, forceWaitForFences
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }
    var maxDeviceMemoryValue by remember { mutableStateOf(config.get("maxDeviceMemory", "0")) }
    var maxDeviceMemoryMenuOpen by remember { mutableStateOf(false) }
    var enableDirectRendering by remember { mutableStateOf(config.getBoolean("useHWBuf", true)) }
    var syncEveryFrame by remember { mutableStateOf(config.getBoolean("forceWaitForFences")) }

    val maxDeviceMemoryLabel = remember(maxDeviceMemoryValue) { formatMemorySizeValueToLabel(maxDeviceMemoryValue) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Turnip configuration") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                Text("Version: $fixedVersion", style = MaterialTheme.typography.labelMedium)

                Text("Max Device Memory", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { maxDeviceMemoryMenuOpen = true }) { Text(maxDeviceMemoryLabel) }
                    DropdownMenu(expanded = maxDeviceMemoryMenuOpen, onDismissRequest = { maxDeviceMemoryMenuOpen = false }) {
                        maxDeviceMemoryEntries().forEach { label ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    maxDeviceMemoryValue = parseMemorySizeLabelToValue(label)
                                    maxDeviceMemoryMenuOpen = false
                                },
                            )
                        }
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text("Enable direct rendering")
                    Checkbox(checked = enableDirectRendering, onCheckedChange = { enableDirectRendering = it })
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text("Sync every frame")
                    Checkbox(checked = syncEveryFrame, onCheckedChange = { syncEveryFrame = it })
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val newConfig = KeyValueSet()
                    newConfig.put("version", fixedVersion)
                    newConfig.put("maxDeviceMemory", maxDeviceMemoryValue)
                    newConfig.put("useHWBuf", if (enableDirectRendering) "1" else "0")
                    newConfig.put("forceWaitForFences", if (syncEveryFrame) "1" else "0")
                    onConfirm(newConfig.toString())
                }
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
internal fun VortekConfigDialogLite(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    // Upstream keys: adrenotoolsDriver, vkMaxVersion, maxDeviceMemory, imageCacheSize, resourceMemoryType, exposedDeviceExtensions
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }
    var vkMaxVersion by remember { mutableStateOf(config.get("vkMaxVersion", "1.3")) }
    var vkMaxVersionMenuOpen by remember { mutableStateOf(false) }

    var maxDeviceMemoryValue by remember { mutableStateOf(config.get("maxDeviceMemory", "0")) }
    var maxDeviceMemoryMenuOpen by remember { mutableStateOf(false) }

    val vkVersionEntries = remember { listOf("1.0", "1.1", "1.2", "1.3") }
    val maxDeviceMemoryLabel = remember(maxDeviceMemoryValue) { formatMemorySizeValueToLabel(maxDeviceMemoryValue) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Vortek configuration") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                Text(
                    "Vortek version: ${BundledGraphicsDriverVersions.VORTEK}",
                    style = MaterialTheme.typography.labelMedium,
                )
                Text("Adrenotools driver: System (bundled)", style = MaterialTheme.typography.labelMedium)

                Text("Vulkan version", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { vkMaxVersionMenuOpen = true }) { Text(vkMaxVersion) }
                    DropdownMenu(expanded = vkMaxVersionMenuOpen, onDismissRequest = { vkMaxVersionMenuOpen = false }) {
                        vkVersionEntries.forEach { v ->
                            DropdownMenuItem(
                                text = { Text(v) },
                                onClick = {
                                    vkMaxVersion = v
                                    vkMaxVersionMenuOpen = false
                                },
                            )
                        }
                    }
                }

                Text("Max Device Memory", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { maxDeviceMemoryMenuOpen = true }) { Text(maxDeviceMemoryLabel) }
                    DropdownMenu(expanded = maxDeviceMemoryMenuOpen, onDismissRequest = { maxDeviceMemoryMenuOpen = false }) {
                        maxDeviceMemoryEntries().forEach { label ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    maxDeviceMemoryValue = parseMemorySizeLabelToValue(label)
                                    maxDeviceMemoryMenuOpen = false
                                },
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val newConfig = KeyValueSet()
                    newConfig.put("adrenotoolsDriver", "System")
                    newConfig.put("vkMaxVersion", vkMaxVersion)
                    newConfig.put("maxDeviceMemory", maxDeviceMemoryValue)
                    // Keep upstream-required keys stable even if we don't expose them yet.
                    newConfig.put("imageCacheSize", config.get("imageCacheSize", "256"))
                    newConfig.put("resourceMemoryType", config.get("resourceMemoryType", "0"))
                    newConfig.put("exposedDeviceExtensions", config.get("exposedDeviceExtensions", "all"))
                    onConfirm(newConfig.toString())
                }
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

private fun parseGlVersionEntryLabel(value: String): String {
    // Upstream StringUtils.parseNumber on spinner item; entries may be "3.1 (Default)"
    return value.substringBefore(" ").trim()
}

@Composable
internal fun VirGLConfigDialogLite(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    // Upstream VirGLConfigDialog: glVersion, disableVertexArrayBGRA (default true)
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }
    val glVersionEntries = remember {
        listOf(
            "2.0",
            "2.1",
            "3.0",
            "3.1 (Default)",
            "3.2",
            "3.3",
            "4.0",
        )
    }
    val defaultGlVersion = "3.1"

    var glVersionValue by remember {
        mutableStateOf(config.get("glVersion", defaultGlVersion))
    }
    var glVersionMenuOpen by remember { mutableStateOf(false) }
    var disableVertexArrayBgra by remember {
        mutableStateOf(config.getBoolean("disableVertexArrayBGRA", true))
    }

    val selectedGlLabel = remember(glVersionValue) {
        glVersionEntries.firstOrNull { parseGlVersionEntryLabel(it) == glVersionValue }
            ?: glVersionValue
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("VirGL Configuration") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                Text(
                    "VirGL (Mesa): ${BundledGraphicsDriverVersions.VIRGL}",
                    style = MaterialTheme.typography.labelMedium,
                )
                Text("Guest OpenGL compatibility", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { glVersionMenuOpen = true }) { Text(selectedGlLabel) }
                    DropdownMenu(expanded = glVersionMenuOpen, onDismissRequest = { glVersionMenuOpen = false }) {
                        glVersionEntries.forEach { label ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    glVersionValue = parseGlVersionEntryLabel(label)
                                    glVersionMenuOpen = false
                                },
                            )
                        }
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text("Disable vertex array BGRA")
                    Checkbox(
                        checked = disableVertexArrayBgra,
                        onCheckedChange = { disableVertexArrayBgra = it },
                    )
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val newConfig = KeyValueSet()
                    newConfig.put("glVersion", glVersionValue)
                    newConfig.put("disableVertexArrayBGRA", if (disableVertexArrayBgra) "1" else "0")
                    onConfirm(newConfig.toString())
                }
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

private data class GpuCardRow(val name: String, val deviceId: Int, val vendorId: Int)

/** Skip malformed upstream rows whose IDs do not fit the Win32/Wine PCI ID fields. */
private fun JSONObject.optionalPciId(key: String): Int? {
    if (!has(key)) return null
    val raw = get(key)
    val v = when (raw) {
        is Int -> raw.toLong()
        is Long -> raw
        is Double -> raw.toLong()
        is String -> raw.trim().toLongOrNull() ?: return null
        else -> return null
    }
    if (v !in 0..65535L) return null
    return v.toInt()
}

/**
 * Mirrors upstream [com.winlator.contentdialog.WineD3DConfigDialog] KeyValueSet keys.
 * Bundled Wine build ships one WineD3D version ([WineInfo.MAIN_WINE_VERSION]); no download/remove UI.
 */
@Composable
internal fun WineD3DConfigDialogLite(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    val context = LocalContext.current
    val bodyScroll = rememberScrollState()
    val gpuMenuScroll = rememberScrollState()
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }

    val gpuCardsRaw = remember {
        runCatching {
            val json = context.assets.open("gpu_cards.json").bufferedReader().use { it.readText() }
            val arr = JSONArray(json)
            buildList {
                for (i in 0 until arr.length()) {
                    val o = arr.getJSONObject(i)
                    val deviceId = o.optionalPciId("deviceID") ?: continue
                    val vendorId = o.optionalPciId("vendorID") ?: continue
                    val name = o.optString("name", "").trim()
                    if (name.isEmpty()) continue
                    add(GpuCardRow(name, deviceId, vendorId))
                }
            }
        }.getOrElse { emptyList() }
    }
    val gpuCards = remember(gpuCardsRaw) {
        gpuCardsRaw.ifEmpty {
            listOf(GpuCardRow("NVIDIA GeForce GTX 480", 1728, 4318))
        }
    }

    val fixedWineVersion = WineInfo.MAIN_WINE_VERSION

    var ddrawWrapper by remember { mutableStateOf(config.get("ddrawWrapper", DXWrappers.WINED3D)) }
    var renderer by remember { mutableStateOf(config.get("renderer", "gl")) }
    var gpuIdx by remember(initialConfig, gpuCards) {
        val cfg = KeyValueSet(initialConfig)
        val targetId = cfg.getInt("VideoPciDeviceID", 1728)
        mutableStateOf(gpuCards.indexOfFirst { it.deviceId == targetId }.coerceAtLeast(0))
    }
    var offscreenMode by remember {
        val raw = config.get("OffscreenRenderingMode", "fbo").lowercase(Locale.ENGLISH)
        val entry = offscreenRenderingModeEntries().firstOrNull {
            it.first == raw || it.second.lowercase(Locale.ENGLISH) == raw
        }
        mutableStateOf(entry?.first ?: "fbo")
    }
    var videoMemLabel by remember {
        mutableStateOf(labelForMemoryConfig(videoMemorySizeEntries(), config.get("VideoMemorySize", "2048")))
    }
    var csmt by remember { mutableStateOf(config.getInt("csmt", 3) != 0) }
    var strictShader by remember { mutableStateOf(config.getInt("strict_shader_math", 1) != 0) }

    var ddrawMenu by remember { mutableStateOf(false) }
    var rendererMenu by remember { mutableStateOf(false) }
    var gpuMenu by remember { mutableStateOf(false) }
    var offscreenMenu by remember { mutableStateOf(false) }
    var videoMemMenu by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("WineD3D Configuration") },
        text = {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 480.dp)
                    .verticalScroll(bodyScroll),
            ) {
                Text("Version: $fixedWineVersion", style = MaterialTheme.typography.labelMedium)

                Text("DDraw Wrapper", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { ddrawMenu = true }) {
                        Text(ddrawWrapperEntries().firstOrNull { it.first == ddrawWrapper }?.second ?: ddrawWrapper)
                    }
                    DropdownMenu(expanded = ddrawMenu, onDismissRequest = { ddrawMenu = false }) {
                        ddrawWrapperEntries().forEach { (id, label) ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    ddrawWrapper = id
                                    ddrawMenu = false
                                },
                            )
                        }
                    }
                }

                Text("Renderer", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { rendererMenu = true }) {
                        Text(wined3dRendererEntries().firstOrNull { it.first == renderer }?.second ?: renderer)
                    }
                    DropdownMenu(expanded = rendererMenu, onDismissRequest = { rendererMenu = false }) {
                        wined3dRendererEntries().forEach { (id, label) ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    renderer = id
                                    rendererMenu = false
                                },
                            )
                        }
                    }
                }

                Text("GPU Name", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { gpuMenu = true }) {
                        Text(gpuCards.getOrNull(gpuIdx)?.name ?: "…")
                    }
                    DropdownMenu(expanded = gpuMenu, onDismissRequest = { gpuMenu = false }) {
                        Column(Modifier.heightIn(max = 280.dp).verticalScroll(gpuMenuScroll)) {
                            gpuCards.forEachIndexed { idx, card ->
                                DropdownMenuItem(
                                    text = { Text(card.name, maxLines = 2, overflow = TextOverflow.Ellipsis) },
                                    onClick = {
                                        gpuIdx = idx
                                        gpuMenu = false
                                    },
                                )
                            }
                        }
                    }
                }

                Text("Offscreen Rendering Mode", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { offscreenMenu = true }) {
                        Text(offscreenRenderingModeEntries().firstOrNull { it.first == offscreenMode }?.second ?: offscreenMode)
                    }
                    DropdownMenu(expanded = offscreenMenu, onDismissRequest = { offscreenMenu = false }) {
                        offscreenRenderingModeEntries().forEach { (id, label) ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    offscreenMode = id
                                    offscreenMenu = false
                                },
                            )
                        }
                    }
                }

                Text("Video Memory Size", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { videoMemMenu = true }) { Text(videoMemLabel) }
                    DropdownMenu(expanded = videoMemMenu, onDismissRequest = { videoMemMenu = false }) {
                        videoMemorySizeEntries().forEach { label ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    videoMemLabel = label
                                    videoMemMenu = false
                                },
                            )
                        }
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Enable CSMT (Command Stream Multi-Thread)", modifier = Modifier.weight(1f))
                    Checkbox(checked = csmt, onCheckedChange = { csmt = it })
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Enable Strict Shader Math", modifier = Modifier.weight(1f))
                    Checkbox(checked = strictShader, onCheckedChange = { strictShader = it })
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val card = gpuCards.getOrNull(gpuIdx)
                    val newConfig = KeyValueSet()
                    newConfig.put("version", fixedWineVersion)
                    newConfig.put("csmt", if (csmt) "3" else "0")
                    if (ddrawWrapper != DXWrappers.WINED3D) newConfig.put("ddrawWrapper", ddrawWrapper)
                    if (renderer != "gl") newConfig.put("renderer", renderer)
                    if (card != null) {
                        newConfig.put("VideoPciDeviceID", card.deviceId.toString())
                        newConfig.put("VideoPciVendorID", card.vendorId.toString())
                    }
                    val modeLabel = offscreenRenderingModeEntries().firstOrNull { it.first == offscreenMode }?.second ?: "FBO"
                    newConfig.put("OffscreenRenderingMode", modeLabel.lowercase(Locale.ENGLISH))
                    newConfig.put("strict_shader_math", if (strictShader) "1" else "0")
                    newConfig.put("VideoMemorySize", StringUtils.parseMemorySize(videoMemLabel))
                    onConfirm(newConfig.toString())
                },
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

/**
 * Mirrors upstream [com.winlator.contentdialog.DXVKConfigDialog] KeyValueSet keys.
 * DXVK versions match bundled `assets/dxwrapper/dxvk-*.tzst` files; no download/remove UI.
 */
@Composable
internal fun DXVKConfigDialogLite(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    val context = LocalContext.current
    val bodyScroll = rememberScrollState()
    val customGpuScroll = rememberScrollState()
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }

    val versions = remember { listOf("1.10.3", "2.4.1") }
    var version by remember {
        val v = config.get("version", "").trim()
        mutableStateOf(versions.firstOrNull { it == v } ?: "2.4.1")
    }
    var ddrawWrapper by remember { mutableStateOf(config.get("ddrawWrapper", DXWrappers.WINED3D)) }
    var framerateLabel by remember {
        val stored = config.get("framerate", "0")
        mutableStateOf(
            dxvkFramerateEntries().firstOrNull { StringUtils.parseNumber(it) == stored }
                ?: dxvkFramerateEntries().first(),
        )
    }
    var maxDeviceMemLabel by remember {
        mutableStateOf(labelForMemoryConfig(maxDeviceMemoryEntries(), config.get("maxDeviceMemory", "0")))
    }

    val gpuCardsWithNone = remember {
        runCatching {
            val json = context.assets.open("gpu_cards.json").bufferedReader().use { it.readText() }
            val arr = JSONArray(json)
            buildList {
                add(GpuCardRow("None", 0, 0))
                for (i in 0 until arr.length()) {
                    val o = arr.getJSONObject(i)
                    val deviceId = o.optionalPciId("deviceID") ?: continue
                    val vendorId = o.optionalPciId("vendorID") ?: continue
                    val name = o.optString("name", "").trim()
                    if (name.isEmpty()) continue
                    add(GpuCardRow(name, deviceId, vendorId))
                }
            }
        }.getOrElse { listOf(GpuCardRow("None", 0, 0)) }
    }

    var customGpuIdx by remember(initialConfig, gpuCardsWithNone) {
        val cfg = KeyValueSet(initialConfig)
        val cd = cfg.get("customDevice", "")
        val idx = if (cd.contains(":")) {
            try {
                val deviceId = Integer.parseInt(cd.split(":")[0], 16)
                gpuCardsWithNone.indexOfFirst { it.deviceId == deviceId }.coerceAtLeast(0)
            } catch (_: Exception) {
                0
            }
        } else {
            0
        }
        mutableStateOf(idx)
    }

    var versionMenu by remember { mutableStateOf(false) }
    var ddrawMenu by remember { mutableStateOf(false) }
    var frameMenu by remember { mutableStateOf(false) }
    var maxMemMenu by remember { mutableStateOf(false) }
    var customGpuMenu by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("DXVK Configuration") },
        text = {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 480.dp)
                    .verticalScroll(bodyScroll),
            ) {
                Text("Version", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { versionMenu = true }) { Text(version) }
                    DropdownMenu(expanded = versionMenu, onDismissRequest = { versionMenu = false }) {
                        versions.forEach { v ->
                            DropdownMenuItem(
                                text = { Text(v) },
                                onClick = {
                                    version = v
                                    versionMenu = false
                                },
                            )
                        }
                    }
                }

                Text("DDraw Wrapper", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { ddrawMenu = true }) {
                        Text(ddrawWrapperEntries().firstOrNull { it.first == ddrawWrapper }?.second ?: ddrawWrapper)
                    }
                    DropdownMenu(expanded = ddrawMenu, onDismissRequest = { ddrawMenu = false }) {
                        ddrawWrapperEntries().forEach { (id, label) ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    ddrawWrapper = id
                                    ddrawMenu = false
                                },
                            )
                        }
                    }
                }

                Text("Frame Rate", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { frameMenu = true }) { Text(framerateLabel) }
                    DropdownMenu(expanded = frameMenu, onDismissRequest = { frameMenu = false }) {
                        dxvkFramerateEntries().forEach { label ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    framerateLabel = label
                                    frameMenu = false
                                },
                            )
                        }
                    }
                }

                Text("Max Device Memory", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { maxMemMenu = true }) { Text(maxDeviceMemLabel) }
                    DropdownMenu(expanded = maxMemMenu, onDismissRequest = { maxMemMenu = false }) {
                        maxDeviceMemoryEntries().forEach { label ->
                            DropdownMenuItem(
                                text = { Text(label) },
                                onClick = {
                                    maxDeviceMemLabel = label
                                    maxMemMenu = false
                                },
                            )
                        }
                    }
                }

                Text("Custom Device", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { customGpuMenu = true }) {
                        Text(gpuCardsWithNone.getOrNull(customGpuIdx)?.name ?: "None")
                    }
                    DropdownMenu(expanded = customGpuMenu, onDismissRequest = { customGpuMenu = false }) {
                        Column(Modifier.heightIn(max = 280.dp).verticalScroll(customGpuScroll)) {
                            gpuCardsWithNone.forEachIndexed { idx, card ->
                                DropdownMenuItem(
                                    text = { Text(card.name, maxLines = 2, overflow = TextOverflow.Ellipsis) },
                                    onClick = {
                                        customGpuIdx = idx
                                        customGpuMenu = false
                                    },
                                )
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val newConfig = KeyValueSet()
                    newConfig.put("version", version)
                    newConfig.put("framerate", StringUtils.parseNumber(framerateLabel))
                    newConfig.put("maxDeviceMemory", parseMemorySizeLabelToValue(maxDeviceMemLabel))
                    if (ddrawWrapper != DXWrappers.WINED3D) newConfig.put("ddrawWrapper", ddrawWrapper)
                    val gpu = gpuCardsWithNone.getOrNull(customGpuIdx)
                    if (gpu != null && gpu.deviceId > 0) {
                        newConfig.put(
                            "customDevice",
                            String.format(Locale.ENGLISH, "%04x", gpu.deviceId) + ":" +
                                String.format(Locale.ENGLISH, "%04x", gpu.vendorId) + ":" + gpu.name,
                        )
                    }
                    onConfirm(newConfig.toString())
                },
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

/**
 * Mirrors upstream [com.winlator.contentdialog.VKD3DConfigDialog] KeyValueSet keys (`version`, `featureLevel`).
 * Single bundled VKD3D build; no download or remove UI.
 */
@Composable
internal fun VKD3DConfigDialogLite(
    initialConfig: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
    bundledVersion: String = BundledDxWrapperArtifacts.VKD3D,
) {
    val config = remember(initialConfig) { KeyValueSet(initialConfig) }
    val defaultFeatureLevel = "12.2"
    var featureLevel by remember {
        val raw = config.get("featureLevel", defaultFeatureLevel).trim()
        mutableStateOf(
            d3dFeatureLevelEntries().firstOrNull { it == raw } ?: defaultFeatureLevel,
        )
    }
    var featureMenuOpen by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("VKD3D Configuration") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                Text("Version: $bundledVersion", style = MaterialTheme.typography.labelMedium)
                Text("D3D Feature Level", style = MaterialTheme.typography.labelMedium)
                Box {
                    TextButton(onClick = { featureMenuOpen = true }) { Text(featureLevel) }
                    DropdownMenu(expanded = featureMenuOpen, onDismissRequest = { featureMenuOpen = false }) {
                        d3dFeatureLevelEntries().forEach { level ->
                            DropdownMenuItem(
                                text = { Text(level) },
                                onClick = {
                                    featureLevel = level
                                    featureMenuOpen = false
                                },
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val newConfig = KeyValueSet()
                    newConfig.put("version", bundledVersion)
                    newConfig.put("featureLevel", featureLevel)
                    onConfirm(newConfig.toString())
                },
            ) { Text("OK") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

