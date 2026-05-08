@file:OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)

package app.trierarch.ui.containers

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.ScrollableTabRow
import androidx.compose.material3.Slider
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.winlator.box64.Box64Preset
import com.winlator.container.Container
import com.winlator.container.DXWrappers
import com.winlator.container.GraphicsDrivers
import com.winlator.core.DefaultVersion
import com.winlator.win32.WinVersions
import kotlinx.coroutines.launch
import org.json.JSONObject

@Composable
fun CreateContainerRoute(
    initialName: String,
    prefillData: JSONObject? = null,
    onDismiss: () -> Unit,
    onCreate: (JSONObject) -> Unit,
) {
    val scope = rememberCoroutineScope()
    val outerScroll = rememberScrollState()
    val pagerState = rememberPagerState(initialPage = ContainerCreateTab.WINE_CONFIGURATION.ordinal) { ContainerCreateTab.values().size }
    val context = LocalContext.current
    val prefillKey = prefillData?.toString() ?: "_new_"
    val initials = remember(prefillKey) { buildCreateContainerFormInitials(context, prefillData, initialName) }

    // ---- Wine Configuration (tab) ----
    var name by remember(prefillKey) { mutableStateOf(initials.name) }
    var screenSize by remember(prefillKey) { mutableStateOf(initials.screenSize) }
    var screenSizeMenuOpen by remember { mutableStateOf(false) }
    var customScreenWidth by remember(prefillKey) { mutableStateOf(initials.customScreenWidth) }
    var customScreenHeight by remember(prefillKey) { mutableStateOf(initials.customScreenHeight) }
    var useCustomScreenSize by remember(prefillKey) { mutableStateOf(initials.useCustomScreenSize) }

    // Upstream shape: Vulkan/OpenGL identifiers + per-api config string, joined with "|" on save.
    val defaultGraphicsDriver = remember(context) { GraphicsDrivers.getDefaultDriver(context) }
    val defaultGraphicsIdentifiers = remember(defaultGraphicsDriver) { GraphicsDrivers.parseIdentifiers(defaultGraphicsDriver) }
    var vulkanDriver by remember(prefillKey) { mutableStateOf(initials.vulkanDriver) }
    var openGLDriver by remember(prefillKey) { mutableStateOf(initials.openGLDriver) }
    var vulkanDriverConfig by remember(prefillKey) { mutableStateOf(initials.vulkanDriverConfig) }
    var openGLDriverConfig by remember(prefillKey) { mutableStateOf(initials.openGLDriverConfig) }
    var graphicsDriverConfigDialogOpen by remember { mutableStateOf(false) }
    var graphicsConfigEditingApi by remember { mutableStateOf("Vulkan") }

    // Upstream shape: Direct3D wrapper is stored in "dxwrapper"; DirectX12 uses VKD3D and second config segment.
    var direct3DWrapper by remember(prefillKey) { mutableStateOf(initials.direct3DWrapper) }
    var direct3DWrapperConfig by remember(prefillKey) { mutableStateOf(initials.direct3DWrapperConfig) }
    var directX12WrapperConfig by remember(prefillKey) { mutableStateOf(initials.directX12WrapperConfig) }
    var dxwrapperConfigDialogOpen by remember { mutableStateOf(false) }
    var dxwrapperConfigEditingApi by remember { mutableStateOf("Direct3D") }

    var audioDriver by remember(prefillKey) { mutableStateOf(initials.audioDriver) }
    var audioDriverConfig by remember(prefillKey) { mutableStateOf(initials.audioDriverConfig) }
    var audioConfigDialogOpen by remember { mutableStateOf(false) }

    var hudMode by remember(prefillKey) { mutableStateOf(initials.hudMode) }
    var hudModeMenuOpen by remember { mutableStateOf(false) }

    var startupSelection by remember(prefillKey) { mutableStateOf(initials.startupSelection) }
    var startupSelectionMenuOpen by remember { mutableStateOf(false) }

    // Upstream desktopTheme shape: "THEME,BACKGROUND_TYPE,#RRGGBB[,wallpaperIdOrTimestamp]"
    var desktopThemeTheme by remember(prefillKey) { mutableStateOf(initials.desktopThemeTheme) } // LIGHT|DARK
    var desktopBackgroundType by remember(prefillKey) { mutableStateOf(initials.desktopBackgroundType) } // IMAGE|COLOR
    var desktopBackgroundColor by remember(prefillKey) { mutableStateOf(initials.desktopBackgroundColor) }
    var desktopWallpaperId by remember(prefillKey) { mutableStateOf(initials.desktopWallpaperId) } // "wallpaper-1"... or "0"
    var systemFont by remember(prefillKey) { mutableStateOf(initials.systemFont) }
    var logPixels by remember(prefillKey) { mutableStateOf(initials.logPixels) }
    var mouseWarpOverride by remember(prefillKey) { mutableStateOf(initials.mouseWarpOverride) } // disable|enable|force

    // ---- WinComponents (tab) ----
    val winComponentRows = remember(prefillKey) {
        val map = mutableStateMapOf<String, Int>()
        for ((k, v) in initials.winComponentMap) {
            map[k] = v
        }
        map
    }

    // ---- EnvVars (tab) ----
    val envVarRows = remember(prefillKey) {
        val list = mutableStateListOf<EnvVarRow>()
        for (row in initials.envVarRows) {
            list.add(EnvVarRow(row.key, row.value))
        }
        list
    }

    // ---- Drives (tab) ----
    val driveRows = remember(prefillKey) {
        val list = mutableStateListOf<DriveRow>()
        for (row in initials.driveRows) {
            list.add(DriveRow(row.letter, row.path))
        }
        list
    }

    // ---- Advanced (tab) ----
    var box64Preset by remember(prefillKey) { mutableStateOf(initials.box64Preset) }
    var box64PresetMenuOpen by remember { mutableStateOf(false) }

    val cpuCount = remember { Runtime.getRuntime().availableProcessors().coerceAtLeast(1) }
    val cpuChecked = remember(prefillKey) { mutableStateOf(initials.cpuChecked.copyOf()) }
    val cpuCheckedWoW64 = remember(prefillKey) { mutableStateOf(initials.cpuCheckedWoW64.copyOf()) }
    var winVersionIdx by remember(prefillKey) { mutableStateOf(initials.winVersionIdx) }
    var winVersionMenuOpen by remember { mutableStateOf(false) }

    fun buildEnvVarsString(): String {
        return envVarRows
            .mapNotNull { row ->
                val k = row.key.trim()
                if (k.isEmpty()) null else "${k}=${row.value}"
            }
            .joinToString(" ")
    }

    fun buildWinComponentsString(): String {
        // Keep default order.
        val keys = Container.DEFAULT_WINCOMPONENTS.split(",").mapNotNull {
            val idx = it.indexOf("=")
            if (idx <= 0) null else it.substring(0, idx)
        }
        return keys.joinToString(",") { key -> "$key=" + (winComponentRows[key] ?: 0) }
    }

    fun buildDrivesString(): String {
        // Upstream format: concatenate segments like "D:" + pathWithoutColon (no delimiter).
        val sb = StringBuilder()
        for (row in driveRows) {
            val letter = row.letter.trim().uppercase().take(1)
            val path = row.path.replace(":", "").trim()
            if (letter.isEmpty() || path.isEmpty()) continue
            sb.append(letter).append(":").append(path)
        }
        return sb.toString()
    }

    fun buildCpuListString(checked: BooleanArray): String {
        val ids = mutableListOf<String>()
        for (i in checked.indices) if (checked[i]) ids.add(i.toString())
        return ids.joinToString(",")
    }

    fun buildScreenSizeString(): String {
        if (!useCustomScreenSize) return screenSize.trim()
        val w = customScreenWidth.trim()
        val h = customScreenHeight.trim()
        val width = w.toIntOrNull()
        val height = h.toIntOrNull()
        if (width == null || height == null) return Container.DEFAULT_SCREEN_SIZE
        if (width <= 0 || height <= 0) return Container.DEFAULT_SCREEN_SIZE
        // Upstream validation: even dimensions only.
        if ((width % 2) != 0 || (height % 2) != 0) return Container.DEFAULT_SCREEN_SIZE
        return "${width}x${height}"
    }

    fun submit() {
        val data = JSONObject()
        data.put("name", name.trim().ifEmpty { initialName })
        data.put("screenSize", buildScreenSizeString())
        data.put("envVars", buildEnvVarsString())
        data.put("cpuList", buildCpuListString(cpuChecked.value))
        data.put("cpuListWoW64", buildCpuListString(cpuCheckedWoW64.value))
        data.put("graphicsDriver", "${vulkanDriver.trim()},${openGLDriver.trim()}")
        val normalizedVulkanGraphicsCfg =
            if (vulkanDriverConfig.trim().isEmpty() && openGLDriverConfig.trim().isNotEmpty()) {
                // Keep the config string well-formed (avoid leading '|') so future parsing/caching logic can rely on
                // the Vulkan segment existing when the OpenGL segment is present.
                // Use the selected Vulkan driver's bundled version as a minimal, stable default.
                "version=" + DefaultVersion.valueOf(vulkanDriver.trim())
            } else {
                vulkanDriverConfig.trim()
            }
        data.put("graphicsDriverConfig", "${normalizedVulkanGraphicsCfg}|${openGLDriverConfig.trim()}")
        data.put("dxwrapper", direct3DWrapper.trim())
        val normalizedDxvkCfg =
            if (direct3DWrapper.trim() == DXWrappers.DXVK && direct3DWrapperConfig.trim().isEmpty()) {
                // Avoid "|vkd3d=..." (DXVK segment empty) which makes DXVK appear "inactive" until the user opens
                // the DXVK config dialog once. Keep minimal defaults; advanced options live in the config dialog.
                "version=" + DefaultVersion.DXVK(vulkanDriver.trim())
            } else {
                direct3DWrapperConfig.trim()
            }
        data.put("dxwrapperConfig", "${normalizedDxvkCfg}|${directX12WrapperConfig.trim()}")
        data.put("audioDriver", audioDriver.trim())
        data.put("audioDriverConfig", audioDriverConfig.trim())
        data.put("wincomponents", buildWinComponentsString())
        data.put("drives", buildDrivesString())
        data.put("hudMode", hudMode)
        data.put("startupSelection", startupSelection)
        data.put("box64Preset", box64Preset.trim())
        val desktopTheme = buildString {
            append(desktopThemeTheme.trim().ifEmpty { "LIGHT" })
            append(",")
            append(desktopBackgroundType.trim().ifEmpty { "IMAGE" })
            append(",")
            append(desktopBackgroundColor.trim().ifEmpty { "#0277bd" })
            if (desktopBackgroundType == "IMAGE") {
                append(",")
                append(desktopWallpaperId.trim().ifEmpty { "0" })
            }
        }
        data.put("desktopTheme", desktopTheme)
        data.put(
            "registry",
            JSONObject()
                .put("systemFont", systemFont)
                .put("logPixels", logPixels)
                .put("mouseWarpOverride", mouseWarpOverride)
                .put("winVersionIdx", winVersionIdx),
        )
        mergePrefillContainerIdentity(prefillData, data)
        onCreate(data)
    }

    val isEdit = prefillData != null
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(if (isEdit) "Edit container" else "New container") },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(outerScroll),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                CreateContainerBaseSection(
                    name = name,
                    onNameChange = { name = it },
                    screenSize = screenSize,
                    onScreenSizeChange = { screenSize = it },
                    screenSizeMenuOpen = screenSizeMenuOpen,
                    onScreenSizeMenuOpenChange = { screenSizeMenuOpen = it },
                    useCustomScreenSize = useCustomScreenSize,
                    onUseCustomScreenSizeChange = { useCustomScreenSize = it },
                    customScreenWidth = customScreenWidth,
                    onCustomScreenWidthChange = { customScreenWidth = it },
                    customScreenHeight = customScreenHeight,
                    onCustomScreenHeightChange = { customScreenHeight = it },
                    hudMode = hudMode,
                    onHudModeChange = { hudMode = it },
                    hudModeMenuOpen = hudModeMenuOpen,
                    onHudModeMenuOpenChange = { hudModeMenuOpen = it },
                    vulkanDriver = vulkanDriver,
                    onVulkanDriverChange = { vulkanDriver = it },
                    openGLDriver = openGLDriver,
                    onOpenGLDriverChange = { id ->
                        openGLDriver = id
                        if (id == GraphicsDrivers.ZINK || id == GraphicsDrivers.GLADIO) {
                            openGLDriverConfig = ""
                        }
                    },
                    onOpenGraphicsConfig = { api ->
                        graphicsConfigEditingApi = api
                        graphicsDriverConfigDialogOpen = true
                    },
                    direct3DWrapper = direct3DWrapper,
                    onDirect3DWrapperChange = { direct3DWrapper = it },
                    onOpenDxWrapperConfig = { api ->
                        dxwrapperConfigEditingApi = api
                        dxwrapperConfigDialogOpen = true
                    },
                    audioDriver = audioDriver,
                    onAudioDriverChange = { audioDriver = it },
                    onOpenAudioConfig = { audioConfigDialogOpen = true },
                )

                ScrollableTabRow(
                    selectedTabIndex = pagerState.currentPage,
                    edgePadding = 0.dp,
                ) {
                    ContainerCreateTab.values().forEachIndexed { idx, t ->
                        Tab(
                            selected = pagerState.currentPage == idx,
                            onClick = { scope.launch { pagerState.animateScrollToPage(idx) } },
                            text = { Text(t.title, maxLines = 1, overflow = TextOverflow.Ellipsis) },
                        )
                    }
                }

                Spacer(Modifier.height(8.dp))

                HorizontalPager(
                    state = pagerState,
                    modifier = Modifier
                        .fillMaxWidth()
                        .wrapContentHeight(),
                ) { page ->
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(4.dp),
                        verticalArrangement = Arrangement.spacedBy(12.dp),
                    ) {
                        when (ContainerCreateTab.values()[page]) {
                            ContainerCreateTab.WINE_CONFIGURATION -> {
                                // --- Desktop (theme/background/font/dpi) ---
                                Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                                    Text("Theme", style = MaterialTheme.typography.labelMedium)
                                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                                        TextButton(onClick = { desktopThemeTheme = "LIGHT" }) {
                                            Text(if (desktopThemeTheme == "LIGHT") "Light ✓" else "Light")
                                        }
                                        TextButton(onClick = { desktopThemeTheme = "DARK" }) {
                                            Text(if (desktopThemeTheme == "DARK") "Dark ✓" else "Dark")
                                        }
                                    }

                                    Text("Background", style = MaterialTheme.typography.labelMedium)
                                    var bgTypeMenuOpen by remember { mutableStateOf(false) }
                                    val bgTypeLabel = when (desktopBackgroundType) {
                                        "COLOR" -> "Solid color"
                                        else -> "Image"
                                    }
                                    Box {
                                        TextButton(onClick = { bgTypeMenuOpen = true }) { Text(bgTypeLabel) }
                                        DropdownMenu(expanded = bgTypeMenuOpen, onDismissRequest = { bgTypeMenuOpen = false }) {
                                            listOf("IMAGE" to "Image", "COLOR" to "Solid color").forEach { (id, label) ->
                                                DropdownMenuItem(
                                                    text = { Text(label) },
                                                    onClick = {
                                                        desktopBackgroundType = id
                                                        bgTypeMenuOpen = false
                                                    },
                                                )
                                            }
                                        }
                                    }

                                    if (desktopBackgroundType == "COLOR") {
                                        OutlinedTextField(
                                            value = desktopBackgroundColor,
                                            onValueChange = { v -> desktopBackgroundColor = v.trim() },
                                            label = { Text("Background color (#RRGGBB)") },
                                            singleLine = true,
                                            modifier = Modifier.fillMaxWidth(),
                                        )
                                    } else {
                                        var wpMenuOpen by remember { mutableStateOf(false) }
                                        val wallpaperEntries = remember { listOf("0", "wallpaper-1", "wallpaper-2", "wallpaper-3") }
                                        val wpLabel = if (desktopWallpaperId == "0") "Default" else desktopWallpaperId
                                        Text("Wallpaper", style = MaterialTheme.typography.labelMedium)
                                        Box {
                                            TextButton(onClick = { wpMenuOpen = true }) { Text(wpLabel) }
                                            DropdownMenu(expanded = wpMenuOpen, onDismissRequest = { wpMenuOpen = false }) {
                                                wallpaperEntries.forEach { id ->
                                                    DropdownMenuItem(
                                                        text = { Text(if (id == "0") "Default" else id) },
                                                        onClick = {
                                                            desktopWallpaperId = id
                                                            wpMenuOpen = false
                                                        },
                                                    )
                                                }
                                            }
                                        }
                                    }

                                    Text("System font", style = MaterialTheme.typography.labelMedium)
                                    var fontMenuOpen by remember { mutableStateOf(false) }
                                    val systemFonts = remember {
                                        listOf(
                                            "Arial",
                                            "Andale Mono",
                                            "Comic Sans MS",
                                            "Courier New",
                                            "Georgia",
                                            "Impact",
                                            "Tahoma",
                                            "Times New Roman",
                                            "Trebuchet MS",
                                            "Verdana",
                                        )
                                    }
                                    Box {
                                        TextButton(onClick = { fontMenuOpen = true }) { Text(systemFont) }
                                        DropdownMenu(expanded = fontMenuOpen, onDismissRequest = { fontMenuOpen = false }) {
                                            systemFonts.forEach { f ->
                                                DropdownMenuItem(
                                                    text = { Text(f) },
                                                    onClick = {
                                                        systemFont = f
                                                        fontMenuOpen = false
                                                    },
                                                )
                                            }
                                        }
                                    }

                                    Text("DPI (Font Size) (${logPixels} dpi)", style = MaterialTheme.typography.labelMedium)
                                    Slider(
                                        value = logPixels.toFloat(),
                                        onValueChange = { v: Float ->
                                            val stepped = (v / 24f).toInt() * 24
                                            logPixels = stepped.coerceIn(96, 240)
                                        },
                                        valueRange = 96f..240f,
                                        steps = 5,
                                    )
                                }

                                // --- DirectInput ---
                                Column {
                                    Text("Mouse Warp Override", style = MaterialTheme.typography.labelMedium)
                                    var mwMenuOpen by remember { mutableStateOf(false) }
                                    val mwLabel = when (mouseWarpOverride) {
                                        "enable" -> "Enable"
                                        "force" -> "Force"
                                        else -> "Disable"
                                    }
                                    Box {
                                        TextButton(onClick = { mwMenuOpen = true }) { Text(mwLabel) }
                                        DropdownMenu(expanded = mwMenuOpen, onDismissRequest = { mwMenuOpen = false }) {
                                            listOf("disable" to "Disable", "enable" to "Enable", "force" to "Force").forEach { (id, label) ->
                                                DropdownMenuItem(
                                                    text = { Text(label) },
                                                    onClick = {
                                                        mouseWarpOverride = id
                                                        mwMenuOpen = false
                                                    },
                                                )
                                            }
                                        }
                                    }
                                }
                            }

                            ContainerCreateTab.WIN_COMPONENTS -> {
                                val orderedKeys = Container.DEFAULT_WINCOMPONENTS.split(",").mapNotNull {
                                    val idx = it.indexOf("=")
                                    if (idx <= 0) null else it.substring(0, idx)
                                }
                                val directXKeys = orderedKeys.filter { winComponentSectionForKey(it) == WinComponentSection.DIRECTX }
                                val generalKeys = orderedKeys.filter { winComponentSectionForKey(it) == WinComponentSection.GENERAL }

                                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Column {
                                        Text("DirectX", style = MaterialTheme.typography.titleSmall)
                                        Spacer(Modifier.height(4.dp))
                                        WinComponentOptionRows(
                                            keys = directXKeys,
                                            selectedIndex = { k -> winComponentRows[k] ?: 0 },
                                            onSelect = { k, idx -> winComponentRows[k] = idx },
                                        )
                                    }
                                    Column {
                                        Text("General", style = MaterialTheme.typography.titleSmall)
                                        Spacer(Modifier.height(4.dp))
                                        WinComponentOptionRows(
                                            keys = generalKeys,
                                            selectedIndex = { k -> winComponentRows[k] ?: 0 },
                                            onSelect = { k, idx -> winComponentRows[k] = idx },
                                        )
                                    }
                                }
                            }

                            ContainerCreateTab.ENV_VARS -> {
                                Text("Environment variables", style = MaterialTheme.typography.titleMedium)
                                envVarRows.forEachIndexed { idx, row ->
                                    Row(
                                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                                        modifier = Modifier.fillMaxWidth(),
                                    ) {
                                        OutlinedTextField(
                                            value = row.key,
                                            onValueChange = { v -> envVarRows[idx] = row.copy(key = v) },
                                            label = { Text("Key") },
                                            singleLine = true,
                                            modifier = Modifier.weight(1f),
                                        )
                                        OutlinedTextField(
                                            value = row.value,
                                            onValueChange = { v -> envVarRows[idx] = row.copy(value = v) },
                                            label = { Text("Value") },
                                            singleLine = true,
                                            modifier = Modifier.weight(1f),
                                        )
                                        IconButton(onClick = { envVarRows.removeAt(idx) }) {
                                            Icon(Icons.Default.Delete, contentDescription = "Remove")
                                        }
                                    }
                                }
                                TextButton(onClick = { envVarRows.add(EnvVarRow("", "")) }) {
                                    Icon(Icons.Default.Add, contentDescription = "Add")
                                    Spacer(Modifier.width(6.dp))
                                    Text("Add")
                                }
                            }

                            ContainerCreateTab.DRIVES -> {
                                Text("Drives", style = MaterialTheme.typography.titleMedium)
                                val driveLetters = remember {
                                    List(Container.MAX_DRIVE_LETTERS.toInt()) { i ->
                                        "${('D'.code + i).toChar()}:"
                                    }
                                }
                                driveRows.forEachIndexed { idx, row ->
                                    var letterMenuOpen by remember(idx) { mutableStateOf(false) }
                                    Row(
                                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                                        modifier = Modifier.fillMaxWidth(),
                                    ) {
                                        Box(modifier = Modifier.weight(0.35f)) {
                                            TextButton(onClick = { letterMenuOpen = true }) {
                                                Text((row.letter.trim().uppercase().take(1).ifEmpty { "D" }) + ":")
                                            }
                                            DropdownMenu(expanded = letterMenuOpen, onDismissRequest = { letterMenuOpen = false }) {
                                                driveLetters.forEach { v ->
                                                    DropdownMenuItem(
                                                        text = { Text(v) },
                                                        onClick = {
                                                            driveRows[idx] = row.copy(letter = v.take(1))
                                                            letterMenuOpen = false
                                                        },
                                                    )
                                                }
                                            }
                                        }
                                        OutlinedTextField(
                                            value = row.path,
                                            onValueChange = { v -> driveRows[idx] = row.copy(path = v) },
                                            label = { Text("Path") },
                                            singleLine = true,
                                            modifier = Modifier.weight(1f),
                                        )
                                        IconButton(onClick = { driveRows.removeAt(idx) }) {
                                            Icon(Icons.Default.Delete, contentDescription = "Remove")
                                        }
                                    }
                                }
                                TextButton(
                                    onClick = {
                                        if (driveRows.size >= Container.MAX_DRIVE_LETTERS) return@TextButton
                                        val nextLetter = ('D'.code + driveRows.size).toChar().toString()
                                        driveRows.add(DriveRow(nextLetter, ""))
                                    }
                                ) {
                                    Icon(Icons.Default.Add, contentDescription = "Add")
                                    Text("Add drive")
                                }
                            }

                            ContainerCreateTab.ADVANCED -> {
                                Column {
                                    Text("Box64 preset", style = MaterialTheme.typography.labelMedium)
                                    Spacer(Modifier.height(6.dp))
                                    TextButton(onClick = { box64PresetMenuOpen = true }) { Text(box64Preset) }
                                    DropdownMenu(expanded = box64PresetMenuOpen, onDismissRequest = { box64PresetMenuOpen = false }) {
                                        listOf(Box64Preset.CONSERVATIVE, Box64Preset.INTERMEDIATE, Box64Preset.PERFORMANCE, Box64Preset.STABILITY).forEach { v ->
                                            DropdownMenuItem(
                                                text = { Text(v) },
                                                onClick = {
                                                    box64Preset = v
                                                    box64PresetMenuOpen = false
                                                },
                                            )
                                        }
                                    }
                                }

                                Column {
                                    Text("Startup selection", style = MaterialTheme.typography.labelMedium)
                                    Spacer(Modifier.height(6.dp))
                                    TextButton(onClick = { startupSelectionMenuOpen = true }) {
                                        Text(
                                            when (startupSelection) {
                                                Container.STARTUP_SELECTION_NORMAL.toInt() -> "Normal"
                                                Container.STARTUP_SELECTION_AGGRESSIVE.toInt() -> "Aggressive"
                                                else -> "Essential"
                                            }
                                        )
                                    }
                                    DropdownMenu(expanded = startupSelectionMenuOpen, onDismissRequest = { startupSelectionMenuOpen = false }) {
                                        listOf(
                                            Container.STARTUP_SELECTION_NORMAL.toInt() to "Normal",
                                            Container.STARTUP_SELECTION_ESSENTIAL.toInt() to "Essential",
                                            Container.STARTUP_SELECTION_AGGRESSIVE.toInt() to "Aggressive",
                                        ).forEach { (id, label) ->
                                            DropdownMenuItem(
                                                text = { Text(label) },
                                                onClick = {
                                                    startupSelection = id
                                                    startupSelectionMenuOpen = false
                                                },
                                            )
                                        }
                                    }
                                }

                                Column {
                                    Text("Windows version", style = MaterialTheme.typography.labelMedium)
                                    Spacer(Modifier.height(6.dp))
                                    TextButton(onClick = { winVersionMenuOpen = true }) {
                                        Text(WinVersions.getWinVersions()[winVersionIdx].description)
                                    }
                                    DropdownMenu(expanded = winVersionMenuOpen, onDismissRequest = { winVersionMenuOpen = false }) {
                                        WinVersions.getWinVersions().forEachIndexed { idx, v ->
                                            DropdownMenuItem(
                                                text = { Text(v.description) },
                                                onClick = {
                                                    winVersionIdx = idx
                                                    winVersionMenuOpen = false
                                                },
                                            )
                                        }
                                    }
                                }

                                Text("CPU list", style = MaterialTheme.typography.titleMedium)
                                for (i in 0 until cpuCount) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                    ) {
                                        Text("CPU $i")
                                        Checkbox(
                                            checked = cpuChecked.value[i],
                                            onCheckedChange = { checked ->
                                                val arr = cpuChecked.value.clone()
                                                arr[i] = checked
                                                cpuChecked.value = arr
                                            },
                                        )
                                    }
                                }

                                Spacer(Modifier.height(12.dp))
                                Text("CPU list WoW64", style = MaterialTheme.typography.titleMedium)
                                for (i in 0 until cpuCount) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                    ) {
                                        Text("CPU $i")
                                        Checkbox(
                                            checked = cpuCheckedWoW64.value[i],
                                            onCheckedChange = { checked ->
                                                val arr = cpuCheckedWoW64.value.clone()
                                                arr[i] = checked
                                                cpuCheckedWoW64.value = arr
                                            },
                                        )
                                    }
                                }
                            }
                        }
                    }
                }

                if (graphicsDriverConfigDialogOpen) {
                    val isVulkan = graphicsConfigEditingApi == "Vulkan"
                    if (isVulkan) {
                        when (vulkanDriver.trim()) {
                            GraphicsDrivers.TURNIP -> {
                                TurnipConfigDialogLite(
                                    initialConfig = vulkanDriverConfig,
                                    fixedVersion = BundledGraphicsDriverVersions.TURNIP,
                                    onDismiss = { graphicsDriverConfigDialogOpen = false },
                                    onConfirm = { cfg ->
                                        vulkanDriverConfig = cfg
                                        graphicsDriverConfigDialogOpen = false
                                    },
                                )
                            }

                            GraphicsDrivers.VORTEK -> {
                                VortekConfigDialogLite(
                                    initialConfig = vulkanDriverConfig,
                                    onDismiss = { graphicsDriverConfigDialogOpen = false },
                                    onConfirm = { cfg ->
                                        vulkanDriverConfig = cfg
                                        graphicsDriverConfigDialogOpen = false
                                    },
                                )
                            }

                            else -> {
                                RawConfigDialog(
                                    title = "Graphics driver config (${graphicsConfigEditingApi})",
                                    value = vulkanDriverConfig,
                                    onValueChange = { v -> vulkanDriverConfig = v },
                                    onDismiss = { graphicsDriverConfigDialogOpen = false },
                                )
                            }
                        }
                    }
                    else {
                        when (openGLDriver.trim()) {
                            GraphicsDrivers.VIRGL -> {
                                VirGLConfigDialogLite(
                                    initialConfig = openGLDriverConfig,
                                    onDismiss = { graphicsDriverConfigDialogOpen = false },
                                    onConfirm = { cfg ->
                                        openGLDriverConfig = cfg
                                        graphicsDriverConfigDialogOpen = false
                                    },
                                )
                            }

                            else -> {
                                RawConfigDialog(
                                    title = "Graphics driver config (${graphicsConfigEditingApi})",
                                    value = openGLDriverConfig,
                                    onValueChange = { v -> openGLDriverConfig = v },
                                    onDismiss = { graphicsDriverConfigDialogOpen = false },
                                )
                            }
                        }
                    }
                }

                if (dxwrapperConfigDialogOpen) {
                    val isD3D = dxwrapperConfigEditingApi == "Direct3D"
                    if (isD3D) {
                        when (direct3DWrapper.trim()) {
                            DXWrappers.WINED3D -> WineD3DConfigDialogLite(
                                initialConfig = direct3DWrapperConfig,
                                onDismiss = { dxwrapperConfigDialogOpen = false },
                                onConfirm = { cfg ->
                                    direct3DWrapperConfig = cfg
                                    dxwrapperConfigDialogOpen = false
                                },
                            )

                            DXWrappers.DXVK -> DXVKConfigDialogLite(
                                initialConfig = direct3DWrapperConfig,
                                onDismiss = { dxwrapperConfigDialogOpen = false },
                                onConfirm = { cfg ->
                                    direct3DWrapperConfig = cfg
                                    dxwrapperConfigDialogOpen = false
                                },
                            )

                            else -> RawConfigDialog(
                                title = "DX wrapper config (Direct3D)",
                                value = direct3DWrapperConfig,
                                onValueChange = { direct3DWrapperConfig = it },
                                onDismiss = { dxwrapperConfigDialogOpen = false },
                            )
                        }
                    } else {
                        VKD3DConfigDialogLite(
                            initialConfig = directX12WrapperConfig,
                            bundledVersion = BundledDxWrapperArtifacts.VKD3D,
                            onDismiss = { dxwrapperConfigDialogOpen = false },
                            onConfirm = { cfg ->
                                directX12WrapperConfig = cfg
                                dxwrapperConfigDialogOpen = false
                            },
                        )
                    }
                }

                if (audioConfigDialogOpen) {
                    AudioConfigDialog(
                        initialConfig = audioDriverConfig,
                        onDismiss = { audioConfigDialogOpen = false },
                        onConfirm = { cfg ->
                            audioDriverConfig = cfg
                            audioConfigDialogOpen = false
                        },
                    )
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { submit() }) { Text(if (isEdit) "Save" else "Create") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

