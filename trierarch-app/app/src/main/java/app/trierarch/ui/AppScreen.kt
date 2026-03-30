package app.trierarch.ui

import android.app.Activity
import android.content.Intent
import android.os.Environment
import android.os.Handler
import android.os.Looper
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import app.trierarch.NativeBridge
import app.trierarch.ProgressCallback
import app.trierarch.WaylandBridge
import app.trierarch.input.InputRouteState
import app.trierarch.ui.screens.DisplayScriptDialog
import app.trierarch.ui.screens.InstallScreen
import app.trierarch.ui.screens.MOUSE_MODE_TOUCHPAD
import app.trierarch.ui.screens.TerminalScreen
import app.trierarch.ui.screens.ViewSettingsDialog
import app.trierarch.wayland.WaylandSurfaceView
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun AppScreen(startInTerminal: Boolean = false) {
    /*
     * AppScreen is the app’s runtime coordinator and UI state machine.
     *
     * ### Lifecycle & state (read this before changing startup behavior)
     *
     * States (simplified):
     * - native init: NativeBridge.init(...) succeeds -> we can call other native methods
     * - rootfs missing: download+extract rootfs -> ask user to restart (fresh proot session)
     * - proot running: spawn proot shell under a PTY -> terminal I/O is available
     * - Wayland server running: WaylandBridge.nativeStartServer(runtimeDir)
     * - view: `showWayland=false` shows the terminal; `showWayland=true` shows the compositor Surface
     *
     * Invariants / ordering constraints:
     * - proot must be spawned before starting the Wayland server because Wayland clients live inside
     *   the proot environment and rely on the socket under XDG_RUNTIME_DIR.
     * - Display startup script is injected via PTY stdin (i.e. typed into the proot shell).
     *   It is guarded by nativeHasActiveClients() so we don’t re-run the script once a desktop client
     *   is already connected.
     *
     * Entry modes:
     * - Default launcher entry: may auto-run Display script once, then switch to Wayland view.
     * - Terminal shortcut entry (`startInTerminal=true`): stays in terminal, never auto-runs Display.
     *
     * Menu: [FloatingMenuOrb] (tap) opens a frosted panel near the orb; placement adapts to screen edges (no sidebar).
     */
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var initialized by remember { mutableStateOf(false) }
    var hasRootfs by remember { mutableStateOf(false) }
    var installProgress by remember { mutableStateOf(0 to "Preparing...") }
    var installDone by remember { mutableStateOf(false) }
    var lines by remember { mutableStateOf(listOf<String>()) }
    var partialLine by remember { mutableStateOf("") }
    var inputLine by remember { mutableStateOf("") }
    var prootSpawned by remember { mutableStateOf(false) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var downloadStarted by remember { mutableStateOf(false) }
    var menuOpen by remember { mutableStateOf(false) }
    var showWayland by remember { mutableStateOf(false) }
    var settingsOpen by remember { mutableStateOf(false) }
    var displayScriptDialogOpen by remember { mutableStateOf(false) }
    var mouseMode by remember { mutableStateOf(MOUSE_MODE_TOUCHPAD) }
    var resolutionPercent by remember { mutableStateOf(100) }
    var scalePercent by remember { mutableStateOf(100) }
    var showKeyboardTrigger by remember { mutableStateOf(0) }
    var autoDisplayTriggered by remember { mutableStateOf(false) }
    val prefs = remember(context) { context.getSharedPreferences("trierarch_prefs", 0) }

    /**
     * Display startup contract (shared by auto-start and the main menu "Display" entry):
     *
     * - **Idempotent**: once a Wayland toplevel client is connected, do not re-run the script.
     * - **Execution model**: script is injected into the running proot shell by writing UTF-8 bytes
     *   into the PTY stdin (equivalent to the user typing it and pressing Enter).
     *
     * Preconditions:
     * - proot is spawned (PTY stdin exists)
     * - Wayland server is started (socket exists under runtimeDir; clients can connect)
     */
    fun runDisplayStartupScriptIfNeeded() {
        val hasClients = try {
            WaylandBridge.nativeHasActiveClients()
        } catch (_: Throwable) {
            false
        }
        if (!hasClients) {
            val script = prefs.getString("display_startup_script", "")?.trim()
            if (!script.isNullOrEmpty()) {
                NativeBridge.writeInput((script + "\n").toByteArray(Charsets.UTF_8))
            }
        }
    }

    fun triggerDisplayToggle() {
        if (showWayland) {
            showWayland = false
        } else {
            runDisplayStartupScriptIfNeeded()
            showWayland = true
        }
        menuOpen = false
    }

    LaunchedEffect(startInTerminal) {
        if (startInTerminal) {
            showWayland = false
        } else {
            autoDisplayTriggered = false
        }
    }

    LaunchedEffect(Unit) {
        mouseMode = prefs.getInt("mouse_mode", MOUSE_MODE_TOUCHPAD)
        resolutionPercent = prefs.getInt("resolution_percent", 100).coerceIn(10, 100)
        scalePercent = prefs.getInt("scale_percent", 100).coerceIn(10, 100)
    }

    fun persistMouseMode(mode: Int) {
        mouseMode = mode
        prefs.edit().putInt("mouse_mode", mode).apply()
    }

    fun persistResolutionPercent(pct: Int) {
        resolutionPercent = pct.coerceIn(10, 100)
        prefs.edit().putInt("resolution_percent", resolutionPercent).apply()
    }

    fun persistScalePercent(pct: Int) {
        scalePercent = pct.coerceIn(10, 100)
        prefs.edit().putInt("scale_percent", scalePercent).apply()
    }

    LaunchedEffect(Unit) {
        if (NativeBridge.init(
                context.filesDir.absolutePath,
                context.cacheDir.absolutePath,
                context.applicationInfo.nativeLibraryDir,
                Environment.getExternalStorageDirectory()?.absolutePath
            )
        ) {
            initialized = true
            hasRootfs = NativeBridge.hasRootfs()
        } else {
            errorMsg = "Failed to initialize native layer"
        }
    }

    LaunchedEffect(initialized, hasRootfs) {
        if (!initialized) return@LaunchedEffect
        if (hasRootfs) {
            if (NativeBridge.spawnProot()) {
                prootSpawned = true
            } else {
                errorMsg = "Failed to spawn proot"
            }
        } else if (!installDone && !downloadStarted) {
            downloadStarted = true
            val mainHandler = Handler(Looper.getMainLooper())
            scope.launch(Dispatchers.IO) {
                val ok = NativeBridge.downloadRootfs(object : ProgressCallback {
                    override fun onProgress(pct: Int, msg: String) {
                        mainHandler.post { installProgress = pct to msg }
                    }
                })
                withContext(Dispatchers.Main) {
                    if (ok) {
                        installDone = true
                    } else {
                        errorMsg = "Download failed"
                        downloadStarted = false
                    }
                }
            }
        }
    }

    val waylandRuntimeDir = remember(context) {
        File(context.filesDir, "usr/tmp").apply { mkdirs() }.absolutePath
    }

    LaunchedEffect(showWayland) {
        InputRouteState.waylandVisible = showWayland
        setImmersiveMode(context as? Activity, showWayland)
    }

    LaunchedEffect(prootSpawned, startInTerminal) {
        if (!prootSpawned) return@LaunchedEffect

        // Always start the Wayland server once proot is ready.
        val keymapTarget = File(waylandRuntimeDir, "keymap_us.xkb")
        if (!keymapTarget.exists()) {
            context.assets.open("keymap_us.xkb").use { input ->
                keymapTarget.outputStream().use { out ->
                    input.copyTo(out)
                }
            }
        }
        try {
            WaylandBridge.nativeStartServer(waylandRuntimeDir)
        } catch (_: Throwable) {
            // If native libs are missing or init fails, stay in terminal.
            return@LaunchedEffect
        }

        if (startInTerminal) return@LaunchedEffect

        // Default launch behavior (MAIN/LAUNCHER): enter Wayland view.
        // Only auto-run Display startup script when it is explicitly configured.
        if (!autoDisplayTriggered) {
            autoDisplayTriggered = true

            val script = prefs.getString("display_startup_script", "")?.trim()
            if (!script.isNullOrEmpty()) {
                runDisplayStartupScriptIfNeeded()
            }
        }

        showWayland = true
    }

    LaunchedEffect(prootSpawned) {
        if (!prootSpawned) return@LaunchedEffect
        var lastLines = listOf<String>()
        var lastPartial = ""
        while (true) {
            val newLines = NativeBridge.getLines()
            val newPartial = NativeBridge.getPartialLine()
            if (newLines != lastLines || newPartial != lastPartial) {
                lastLines = newLines
                lastPartial = newPartial
                lines = newLines
                partialLine = newPartial
            }
            delay(100)
        }
    }

    errorMsg?.let { msg ->
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text(msg, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    if (!initialized) {
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            CircularProgressIndicator(color = MaterialTheme.colorScheme.primary)
        }
        return
    }

    if (!hasRootfs && !installDone) {
        InstallScreen(progress = installProgress.first, message = installProgress.second)
        return
    }

    if (installDone) {
        AlertDialog(
            onDismissRequest = { },
            title = { Text("Setup complete") },
            text = { Text("Restart the app to open the terminal.") },
            confirmButton = {
                TextButton(onClick = {
                    val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)?.apply {
                        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                    }
                    if (intent != null) {
                        context.startActivity(intent)
                        (context as? Activity)?.finish()
                    }
                }) { Text("Restart") }
            },
            dismissButton = {
                TextButton(onClick = { (context as? Activity)?.finish() }) { Text("Exit") }
            }
        )
        return
    }

    if (hasRootfs && !prootSpawned) {
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text(AppStrings.STARTING, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Box(modifier = Modifier.fillMaxSize()) {
            if (showWayland) {
                WaylandSurfaceView(
                    runtimeDir = waylandRuntimeDir,
                    mouseMode = mouseMode,
                    resolutionPercent = resolutionPercent,
                    scalePercent = scalePercent,
                    showKeyboardTrigger = showKeyboardTrigger,
                    onKeyboardTriggerConsumed = { showKeyboardTrigger = 0 },
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                TerminalScreen(
                    lines = lines,
                    partialLine = partialLine,
                    inputLine = inputLine,
                    onInputChange = { inputLine = it },
                    onInputSubmit = {
                        NativeBridge.writeInput((it + "\n").toByteArray(Charsets.UTF_8))
                        inputLine = ""
                    },
                    showKeyboardTrigger = showKeyboardTrigger,
                    onKeyboardTriggerConsumed = { showKeyboardTrigger = 0 }
                )
            }
        }
        FloatingMenuOrb(
            prefs = prefs,
            menuExpanded = menuOpen,
            onMenuOpenRequest = { menuOpen = true },
            onDismissMenu = { menuOpen = false },
            onDisplayClick = { triggerDisplayToggle() },
            onDisplayLongPress = {
                menuOpen = false
                displayScriptDialogOpen = true
            },
            onViewClick = {
                menuOpen = false
                settingsOpen = true
            },
            onKeyboardClick = {
                menuOpen = false
                showKeyboardTrigger += 1
            }
        )
        if (settingsOpen) {
            ViewSettingsDialog(
                onDismiss = { settingsOpen = false },
                mouseMode = mouseMode,
                resolutionPercent = resolutionPercent,
                scalePercent = scalePercent,
                onMouseModeChange = { persistMouseMode(it) },
                onResolutionPercentChange = { persistResolutionPercent(it) },
                onScalePercentChange = { persistScalePercent(it) }
            )
        }
        if (displayScriptDialogOpen) {
            DisplayScriptDialog(
                initialScript = prefs.getString("display_startup_script", "") ?: "",
                onDismiss = { displayScriptDialogOpen = false },
                onConfirm = { script ->
                    prefs.edit().putString("display_startup_script", script).apply()
                    displayScriptDialogOpen = false
                }
            )
        }
    }
}
