package app.trierarch.ui.containers

import android.content.Context
import com.winlator.container.Container
import com.winlator.container.GraphicsDrivers
import com.winlator.core.KeyValueSet
import com.winlator.core.WineInfo
import com.winlator.win32.WinVersions
import org.json.JSONObject

/** Entries must match [CreateContainerBaseSection] screen size dropdown (excluding Custom). */
internal fun containerScreenSizeMenuEntries(): List<String> =
    listOf(
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

internal fun parseContainerScreenMenuLabel(value: String): String =
    value.substringBefore(" ").trim()

internal data class CreateContainerFormInitials(
    val name: String,
    val screenSize: String,
    val useCustomScreenSize: Boolean,
    val customScreenWidth: String,
    val customScreenHeight: String,
    val vulkanDriver: String,
    val openGLDriver: String,
    val vulkanDriverConfig: String,
    val openGLDriverConfig: String,
    val direct3DWrapper: String,
    val direct3DWrapperConfig: String,
    val directX12WrapperConfig: String,
    val audioDriver: String,
    val audioDriverConfig: String,
    val hudMode: Int,
    val startupSelection: Int,
    val desktopThemeTheme: String,
    val desktopBackgroundType: String,
    val desktopBackgroundColor: String,
    val desktopWallpaperId: String,
    val systemFont: String,
    val logPixels: Int,
    val mouseWarpOverride: String,
    val winVersionIdx: Int,
    val winComponentMap: Map<String, Int>,
    val envVarRows: List<EnvVarRow>,
    val driveRows: List<DriveRow>,
    val box64Preset: String,
    val cpuChecked: BooleanArray,
    val cpuCheckedWoW64: BooleanArray,
)

internal fun buildCreateContainerFormInitials(
    context: Context,
    prefill: JSONObject?,
    initialName: String,
): CreateContainerFormInitials {
    val defaultGraphicsDriver = GraphicsDrivers.getDefaultDriver(context)
    val defaultIds = GraphicsDrivers.parseIdentifiers(defaultGraphicsDriver)
    val defaultThemeParts = com.winlator.core.WineThemeManager.DEFAULT_DESKTOP_THEME.split(",")
    val defaultWinVerIdx =
        WinVersions.getWinVersions().indexOfFirst { w -> w.version == WinVersions.DEFAULT_VERSION }.coerceAtLeast(0)

    if (prefill == null) {
        val cpuCount = Runtime.getRuntime().availableProcessors().coerceAtLeast(1)
        val cpuAll = BooleanArray(cpuCount) { true }
        val cpuWow = BooleanArray(cpuCount) { idx -> idx >= cpuCount / 2 }
        val winMap = mutableMapOf<String, Int>()
        for (part in Container.DEFAULT_WINCOMPONENTS.split(",")) {
            val idx = part.indexOf("=")
            if (idx > 0) {
                winMap[part.substring(0, idx)] = part.substring(idx + 1).toIntOrNull() ?: 0
            }
        }
        val envRows = mutableListOf<EnvVarRow>()
        for (item in Container.DEFAULT_ENV_VARS.split(" ")) {
            val idx = item.indexOf("=")
            if (idx > 0) envRows.add(EnvVarRow(item.substring(0, idx), item.substring(idx + 1)))
        }
        val driveList = mutableListOf<DriveRow>()
        for (d in Container.drivesIterator(Container.DEFAULT_DRIVES)) {
            driveList.add(DriveRow(d.letter, d.path))
        }
        return CreateContainerFormInitials(
            name = initialName,
            screenSize = Container.DEFAULT_SCREEN_SIZE,
            useCustomScreenSize = false,
            customScreenWidth = "",
            customScreenHeight = "",
            vulkanDriver = defaultIds[0],
            openGLDriver = defaultIds[1],
            vulkanDriverConfig = "",
            openGLDriverConfig = "",
            direct3DWrapper = Container.DEFAULT_DXWRAPPER,
            direct3DWrapperConfig = "",
            directX12WrapperConfig =
                KeyValueSet().apply {
                    put("version", BundledDxWrapperArtifacts.VKD3D)
                    put("featureLevel", "12.2")
                }.toString(),
            audioDriver = Container.DEFAULT_AUDIO_DRIVER,
            audioDriverConfig = "",
            hudMode = 0,
            startupSelection = Container.STARTUP_SELECTION_ESSENTIAL.toInt(),
            desktopThemeTheme = defaultThemeParts.getOrNull(0)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "LIGHT",
            desktopBackgroundType = defaultThemeParts.getOrNull(1)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "IMAGE",
            desktopBackgroundColor = defaultThemeParts.getOrNull(2)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "#0277bd",
            desktopWallpaperId = defaultThemeParts.getOrNull(3)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "0",
            systemFont = "Tahoma",
            logPixels = 96,
            mouseWarpOverride = "disable",
            winVersionIdx = defaultWinVerIdx,
            winComponentMap = winMap,
            envVarRows = envRows,
            driveRows = driveList,
            box64Preset = com.winlator.box64.Box64Preset.DEFAULT,
            cpuChecked = cpuAll,
            cpuCheckedWoW64 = cpuWow,
        )
    }

    val name = prefill.optString("name", "").trim().ifEmpty { initialName }
    val savedScreen = prefill.optString("screenSize", Container.DEFAULT_SCREEN_SIZE).trim()
        .ifEmpty { Container.DEFAULT_SCREEN_SIZE }
    val entries = containerScreenSizeMenuEntries()
    val matchedPreset = entries.firstOrNull { entry -> parseContainerScreenMenuLabel(entry) == savedScreen }
    val useCustom = matchedPreset == null
    val screenSize = if (useCustom) Container.DEFAULT_SCREEN_SIZE else savedScreen
    val customParts = savedScreen.split("x", limit = 2)
    val customW = if (useCustom) customParts.getOrElse(0) { "" }.trim() else ""
    val customH = if (useCustom) customParts.getOrElse(1) { "" }.trim() else ""

    val graphics = prefill.optString("graphicsDriver", "").trim()
    val gdParts: List<String> =
        graphics.split(",").map { part: String -> part.trim() }.filter { t -> t.isNotEmpty() }
    val vulkan = gdParts.getOrElse(0) { defaultIds[0] }
    val gl = gdParts.getOrElse(1) { defaultIds[1] }

    val gdc = prefill.optString("graphicsDriverConfig", "")
    val gdcParts = gdc.split("|", limit = 2)
    val vulkanCfg = gdcParts.getOrElse(0) { "" }.trim()
    val glCfg = gdcParts.getOrElse(1) { "" }.trim()

    val dxw = prefill.optString("dxwrapper", Container.DEFAULT_DXWRAPPER).trim()
        .ifEmpty { Container.DEFAULT_DXWRAPPER }
    val dxc = prefill.optString("dxwrapperConfig", "")
    val dxcParts = dxc.split("|", limit = 2)
    val d3dCfg = dxcParts.getOrElse(0) { "" }.trim()
    val dx12Cfg = dxcParts.getOrElse(1) { "" }.trim().ifEmpty {
        KeyValueSet().apply {
            put("version", BundledDxWrapperArtifacts.VKD3D)
            put("featureLevel", "12.2")
        }.toString()
    }

    val winMap = mutableMapOf<String, Int>()
    for (part in Container.DEFAULT_WINCOMPONENTS.split(",")) {
        val eq = part.indexOf("=")
        if (eq > 0) {
            val k = part.substring(0, eq)
            winMap[k] = part.substring(eq + 1).toIntOrNull() ?: 0
        }
    }
    try {
        val fromFile = KeyValueSet(prefill.optString("wincomponents", Container.DEFAULT_WINCOMPONENTS))
        for (pair in fromFile) {
            winMap[pair[0]] = pair[1].toIntOrNull() ?: winMap[pair[0]] ?: 0
        }
    } catch (_: Exception) {
    }

    val envRows = mutableListOf<EnvVarRow>()
    val envStr = prefill.optString("envVars", Container.DEFAULT_ENV_VARS)
    for (item in envStr.split(" ")) {
        val idx = item.indexOf("=")
        if (idx > 0) envRows.add(EnvVarRow(item.substring(0, idx), item.substring(idx + 1)))
    }
    if (envRows.isEmpty()) {
        for (item in Container.DEFAULT_ENV_VARS.split(" ")) {
            val idx = item.indexOf("=")
            if (idx > 0) envRows.add(EnvVarRow(item.substring(0, idx), item.substring(idx + 1)))
        }
    }

    val driveList = mutableListOf<DriveRow>()
    val drivesStr = prefill.optString("drives", Container.DEFAULT_DRIVES)
    try {
        for (d in Container.drivesIterator(drivesStr)) {
            driveList.add(DriveRow(d.letter, d.path))
        }
    } catch (_: Exception) {
    }
    if (driveList.isEmpty()) {
        for (d in Container.drivesIterator(Container.DEFAULT_DRIVES)) {
            driveList.add(DriveRow(d.letter, d.path))
        }
    }

    val cpuCount = Runtime.getRuntime().availableProcessors().coerceAtLeast(1)
    fun parseCpu(raw: String?, default: BooleanArray): BooleanArray {
        val arr = BooleanArray(cpuCount) { false }
        if (raw.isNullOrBlank()) return default.copyOf()
        val set = raw.split(",").mapNotNull { token -> token.trim().toIntOrNull() }.toSet()
        if (set.isEmpty()) return default.copyOf()
        for (i in arr.indices) arr[i] = set.contains(i)
        return arr
    }
    val cpuDefault = BooleanArray(cpuCount) { true }
    val cpuWowDefault = BooleanArray(cpuCount) { idx -> idx >= cpuCount / 2 }
    val cpuListRaw = if (prefill.has("cpuList")) prefill.optString("cpuList", "") else null
    val cpuWowRaw = if (prefill.has("cpuListWoW64")) prefill.optString("cpuListWoW64", "") else null
    val cpuMask = parseCpu(cpuListRaw, cpuDefault)
    val cpuMaskWoW64 = parseCpu(cpuWowRaw, cpuWowDefault)

    val themeStr = prefill.optString("desktopTheme", com.winlator.core.WineThemeManager.DEFAULT_DESKTOP_THEME)
    val tparts = themeStr.split(",")
    val dtTheme = tparts.getOrNull(0)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "LIGHT"
    val dtBg = tparts.getOrNull(1)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "IMAGE"
    val dtColor = tparts.getOrNull(2)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "#0277bd"
    val dtWp = tparts.getOrNull(3)?.trim()?.takeIf { s -> s.isNotEmpty() } ?: "0"

    val reg = prefill.optJSONObject("registry")
    val sysFont = reg?.optString("systemFont", "Tahoma")?.trim()?.ifEmpty { "Tahoma" } ?: "Tahoma"
    val lp = reg?.optInt("logPixels", 96) ?: 96
    val mw = reg?.optString("mouseWarpOverride", "disable")?.trim()?.ifEmpty { "disable" } ?: "disable"
    val wvIdx = reg?.optInt("winVersionIdx", -1)?.takeIf { idx -> idx >= 0 }
        ?: defaultWinVerIdx

    val box64 = prefill.optString("box64Preset", com.winlator.box64.Box64Preset.DEFAULT).trim()
        .ifEmpty { com.winlator.box64.Box64Preset.DEFAULT }

    return CreateContainerFormInitials(
        name = name,
        screenSize = screenSize,
        useCustomScreenSize = useCustom,
        customScreenWidth = customW,
        customScreenHeight = customH,
        vulkanDriver = vulkan,
        openGLDriver = gl,
        vulkanDriverConfig = vulkanCfg,
        openGLDriverConfig = glCfg,
        direct3DWrapper = dxw,
        direct3DWrapperConfig = d3dCfg,
        directX12WrapperConfig = dx12Cfg,
        audioDriver = prefill.optString("audioDriver", Container.DEFAULT_AUDIO_DRIVER).trim()
            .ifEmpty { Container.DEFAULT_AUDIO_DRIVER },
        audioDriverConfig = prefill.optString("audioDriverConfig", ""),
        hudMode = prefill.optInt("hudMode", 0),
        startupSelection = prefill.optInt("startupSelection", Container.STARTUP_SELECTION_ESSENTIAL.toInt()),
        desktopThemeTheme = dtTheme,
        desktopBackgroundType = dtBg,
        desktopBackgroundColor = dtColor,
        desktopWallpaperId = dtWp,
        systemFont = sysFont,
        logPixels = lp.coerceIn(96, 240),
        mouseWarpOverride = mw,
        winVersionIdx = wvIdx.coerceIn(0, WinVersions.getWinVersions().size - 1),
        winComponentMap = winMap,
        envVarRows = envRows,
        driveRows = driveList,
        box64Preset = box64,
        cpuChecked = cpuMask,
        cpuCheckedWoW64 = cpuMaskWoW64,
    )
}

/** Copy identifiers from [prefill] into [data] after the form fields are written (edit / preserve non-main Wine). */
internal fun mergePrefillContainerIdentity(prefill: JSONObject?, data: JSONObject) {
    if (prefill == null) return
    if (prefill.has("id")) {
        data.put("id", prefill.getInt("id"))
    }
    // Preserve cached fingerprints and version markers (extraData) when editing.
    // Upstream Winlator edits the existing Container object; our Compose flow rebuilds JSON, so we must carry this over.
    if (prefill.has("extraData")) {
        try {
            data.put("extraData", prefill.getJSONObject("extraData"))
        } catch (_: Throwable) {
            // Ignore malformed extraData; container code will re-populate as needed.
        }
    }
    if (prefill.has("wineVersion")) {
        val v = prefill.getString("wineVersion")
        if (v.isNotEmpty() && !WineInfo.isMainWineVersion(v)) {
            data.put("wineVersion", v)
        }
    }
}
