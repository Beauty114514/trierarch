package app.trierarch.ui

import android.app.Activity
import android.content.Intent
import android.os.Environment
import android.os.Handler
import android.os.Looper
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalContext
import app.trierarch.NativeBridge
import app.trierarch.PulseAssets
import app.trierarch.ProgressCallback
import app.trierarch.TerminalSessionIds
import app.trierarch.VirglAssets
import app.trierarch.WaylandBridge
import app.trierarch.input.InputRouteState
import app.trierarch.shell.ShellFonts
import app.trierarch.ui.dialog.AppearanceDialog
import app.trierarch.ui.dialog.DisplayScriptDialog
import app.trierarch.ui.dialog.SessionsDialog
import app.trierarch.ui.dialog.MOUSE_MODE_TOUCHPAD
import app.trierarch.ui.dialog.ViewSettingsDialog
import app.trierarch.ui.orb.FloatingMenuOrb
import app.trierarch.ui.setup.InstallScreen
import app.trierarch.ui.shell.ShellScreen
import app.trierarch.wayland.WaylandSurfaceView
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private val RENDERER_MODES = listOf("LLVMPIPE", "UNIVERSAL")

/** Maps legacy prefs (VIRGL/VENUS) to UNIVERSAL; second value = prefs key should store UNIVERSAL. */
private fun normalizeDesktopRendererRaw(raw: String): Pair<String, Boolean> {
    val u = raw.trim().uppercase()
    val legacy = u == "VENUS" || u == "VIRGL"
    val token = if (legacy) "UNIVERSAL" else u
    return Pair(token, legacy)
}

private fun audioDefaultSinkRuntimeSnippet(): String =
    "pactl set-default-sink trierarch-out >/dev/null 2>&1 || true"

@Composable
fun AppScreen(startInTerminal: Boolean = false) {
    // Order: NativeBridge.init → rootfs → proot before Wayland server. Shell UI stays in the tree
    // when toggling desktop so terminal buffers survive. Display script uses session 0; renderer
    // changes clear PTYs (env is fixed at spawn).
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var initialized by remember { mutableStateOf(false) }
    var hasRootfs by remember { mutableStateOf(false) }
    var installProgress by remember { mutableStateOf(0 to "Preparing...") }
    var installDone by remember { mutableStateOf(false) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var downloadStarted by remember { mutableStateOf(false) }
    var menuOpen by remember { mutableStateOf(false) }
    var showWayland by remember { mutableStateOf(false) }
    var settingsOpen by remember { mutableStateOf(false) }
    var appearanceOpen by remember { mutableStateOf(false) }
    var displayScriptDialogOpen by remember { mutableStateOf(false) }
    var sessionsDialogOpen by remember { mutableStateOf(false) }
    var terminalSessionIds by remember {
        mutableStateOf(listOf(TerminalSessionIds.FIRST_TERMINAL))
    }
    var activeTerminalSessionId by remember {
        mutableStateOf(TerminalSessionIds.FIRST_TERMINAL)
    }
    var mouseMode by remember { mutableStateOf(MOUSE_MODE_TOUCHPAD) }
    var resolutionPercent by remember { mutableStateOf(100) }
    var scalePercent by remember { mutableStateOf(100) }
    var showKeyboardTrigger by remember { mutableStateOf(0) }
    var keyboardWanted by remember { mutableStateOf(false) }
    var autoDisplayTriggered by remember { mutableStateOf(false) }
    var pendingAutoShowWayland by remember { mutableStateOf(false) }
    var desktopRendererMode by remember { mutableStateOf("LLVMPIPE") }
    /** Incremented when [desktopRendererMode] changes so [ShellScreen] drops cached terminal sessions. */
    var rendererSessionResetEpoch by remember { mutableIntStateOf(0) }
    /** Full-screen cover during default desktop launch or Display toggle so the terminal flash is hidden. */
    var desktopLaunchBlackout by remember(startInTerminal) {
        mutableStateOf(!startInTerminal)
    }
    var terminalFontKey by remember { mutableStateOf(ShellFonts.DEFAULT_ID) }
    val prefs = remember(context) { context.getSharedPreferences("trierarch_prefs", 0) }
    val waylandRuntimeDir = remember(context) {
        File(context.filesDir, "usr/tmp").apply { mkdirs() }.absolutePath
    }

    /**
     * Copy bundled keymap (if missing) and start the in-process Wayland compositor socket + dispatch thread.
     * Idempotent with respect to native `nativeStartServer` (safe to call more than once).
     *
     * Terminal shortcut entry defers this until the user opens **Display**, so only the PTY + terminal view
     * run until then (fewer moving parts when debugging crashes after a delay).
     */
    fun prepareWaylandRuntimeAndStartServer(): Boolean {
        val keymapTarget = File(waylandRuntimeDir, "keymap_us.xkb")
        if (!keymapTarget.exists()) {
            try {
                context.assets.open("keymap_us.xkb").use { input ->
                    keymapTarget.outputStream().use { out ->
                        input.copyTo(out)
                    }
                }
            } catch (_: Throwable) {
                return false
            }
        }
        return try {
            WaylandBridge.nativeStartServer(waylandRuntimeDir)
            true
        } catch (_: Throwable) {
            false
        }
    }

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
    fun ensureDisplaySession() {
        if (!NativeBridge.isSessionAlive(TerminalSessionIds.DISPLAY)) {
            NativeBridge.spawnSession(TerminalSessionIds.DISPLAY, 24, 80)
        }
    }

    fun runDisplayStartupScriptIfNeeded() {
        val hasClients = try {
            WaylandBridge.nativeHasActiveClients()
        } catch (_: Throwable) {
            false
        }
        if (!hasClients) {
            // Run this before the Display script so later startup commands inherit AAudio as default.
            ensureDisplaySession()
            NativeBridge.writeInput(
                TerminalSessionIds.DISPLAY,
                (audioDefaultSinkRuntimeSnippet() + "\n").toByteArray(Charsets.UTF_8)
            )

            val script = prefs.getString("display_startup_script", "")?.trim()
            if (!script.isNullOrEmpty()) {
                ensureDisplaySession()
                NativeBridge.writeInput(
                    TerminalSessionIds.DISPLAY,
                    (script + "\n").toByteArray(Charsets.UTF_8)
                )
            }
        }
    }

    fun triggerDisplayToggle() {
        if (showWayland) {
            showWayland = false
            desktopLaunchBlackout = false
        } else {
            if (!prepareWaylandRuntimeAndStartServer()) {
                menuOpen = false
                return
            }
            desktopLaunchBlackout = true
            runDisplayStartupScriptIfNeeded()
            pendingAutoShowWayland = true
        }
        menuOpen = false
    }

    LaunchedEffect(startInTerminal) {
        if (startInTerminal) {
            showWayland = false
            pendingAutoShowWayland = false
            desktopLaunchBlackout = false
        } else {
            autoDisplayTriggered = false
        }
    }

    LaunchedEffect(Unit) {
        mouseMode = prefs.getInt("mouse_mode", MOUSE_MODE_TOUCHPAD)
        resolutionPercent = prefs.getInt("resolution_percent", 100).coerceIn(10, 100)
        scalePercent = prefs.getInt("scale_percent", 100)
            .coerceIn(100, 1000)
            .let { v -> ((v + 50) / 100) * 100 }
        terminalFontKey = prefs.getString(ShellFonts.PREF_KEY, ShellFonts.DEFAULT_ID)
            ?: ShellFonts.DEFAULT_ID
    }

    fun persistDesktopRendererMode(mode: String) {
        val (token, _) = normalizeDesktopRendererRaw(mode)
        val safe = if (token in RENDERER_MODES) token else "LLVMPIPE"
        val modeChanged = desktopRendererMode != safe
        desktopRendererMode = safe
        prefs.edit().putString("desktop_renderer_mode", safe).apply()
        try {
            NativeBridge.setRendererMode(safe)
        } catch (_: Throwable) {
            // Keep UI responsive even if native layer isn't ready yet.
        }
        if (modeChanged) {
            rendererSessionResetEpoch++
            terminalSessionIds = listOf(TerminalSessionIds.FIRST_TERMINAL)
            activeTerminalSessionId = TerminalSessionIds.FIRST_TERMINAL
        }
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
        scalePercent = pct.coerceIn(100, 1000).let { v -> ((v + 50) / 100) * 100 }
        prefs.edit().putInt("scale_percent", scalePercent).apply()
    }

    fun persistTerminalFont(key: String) {
        terminalFontKey = key
        prefs.edit().putString(ShellFonts.PREF_KEY, key).apply()
    }

    LaunchedEffect(Unit) {
        // Must run before persistDesktopRendererMode: parallel LaunchedEffect(Unit) blocks are unordered;
        // if we used default LLVMPIPE here, native would stay wrong when prefs say UNIVERSAL.
        run {
            val raw = prefs.getString("desktop_renderer_mode", "LLVMPIPE") ?: "LLVMPIPE"
            val (token, legacy) = normalizeDesktopRendererRaw(raw)
            if (legacy) prefs.edit().putString("desktop_renderer_mode", "UNIVERSAL").apply()
            desktopRendererMode = if (token in RENDERER_MODES) token else "LLVMPIPE"
        }
        val ok = withContext(Dispatchers.IO) {
            if (!NativeBridge.init(
                    context.filesDir.absolutePath,
                    context.cacheDir.absolutePath,
                    context.applicationInfo.nativeLibraryDir,
                    Environment.getExternalStorageDirectory()?.absolutePath
                )
            ) {
                return@withContext false
            }
            // Must run before touching files/virgl/: a live virgl child may mmap libs under the
            // tree we rename/delete (otherwise tombstones show "(deleted)" in libepoxy).
            NativeBridge.stopVirglHost()
            PulseAssets.syncFromAssetsIfNeeded(context)
            VirglAssets.syncFromAssetsIfNeeded(context)
            true
        }
        if (ok) {
            initialized = true
            hasRootfs = NativeBridge.hasRootfs()
            // Apply persisted renderer mode to native layer (affects newly spawned proot processes).
            persistDesktopRendererMode(desktopRendererMode)
        } else {
            errorMsg = "Failed to initialize native layer"
        }
    }

    LaunchedEffect(initialized, hasRootfs) {
        if (!initialized) return@LaunchedEffect
        if (!hasRootfs && !installDone && !downloadStarted) {
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

    LaunchedEffect(showWayland) {
        setImmersiveMode(context as? Activity, immersive = showWayland)
        InputRouteState.waylandVisible = showWayland
        if (showWayland) {
            desktopLaunchBlackout = false
            try {
                WaylandBridge.nativeResetKeyboardState()
            } catch (_: Throwable) {
            }
        }
    }

    LaunchedEffect(menuOpen, settingsOpen, appearanceOpen, displayScriptDialogOpen, sessionsDialogOpen) {
        // Don't auto-pop the IME while the user is interacting with menus/dialogs.
        if (menuOpen || settingsOpen || appearanceOpen || displayScriptDialogOpen || sessionsDialogOpen) {
            keyboardWanted = false
        }
    }

    LaunchedEffect(pendingAutoShowWayland) {
        if (!pendingAutoShowWayland) return@LaunchedEffect
        // Wait until a desktop toplevel client is connected before switching to the Wayland view.
        while (pendingAutoShowWayland && !showWayland) {
            val ready = try {
                WaylandBridge.nativeHasActiveClients()
            } catch (_: Throwable) {
                false
            }
            if (ready) {
                // Give the desktop a short extra moment to finish its first frames
                // so that frantic tapping during the very first instant is less likely
                // to leave the user on a black screen.
                delay(350)
                showWayland = true
                pendingAutoShowWayland = false
                break
            }
            delay(100)
        }
    }

    LaunchedEffect(hasRootfs, startInTerminal) {
        if (!hasRootfs) return@LaunchedEffect
        var waited = 0
        while (!NativeBridge.isSessionAlive(TerminalSessionIds.FIRST_TERMINAL) && waited < 256) {
            delay(32)
            waited++
        }
        if (!NativeBridge.isSessionAlive(TerminalSessionIds.FIRST_TERMINAL)) {
            desktopLaunchBlackout = false
            return@LaunchedEffect
        }

        // Terminal shortcut: do not start the in-process Wayland compositor until the user taps **Display**
        // (see [prepareWaylandRuntimeAndStartServer] in [triggerDisplayToggle]). Avoids native crashes while
        // only the PTY terminal is needed and matches "shortcut = terminal first" UX.
        if (startInTerminal) return@LaunchedEffect

        if (!prepareWaylandRuntimeAndStartServer()) {
            desktopLaunchBlackout = false
            return@LaunchedEffect
        }

        // Default launcher: may auto-run Display script, then wait for desktop (ShellScreen stays under [desktopLaunchBlackout]).
        if (!autoDisplayTriggered) {
            autoDisplayTriggered = true
            runDisplayStartupScriptIfNeeded()
        }
        pendingAutoShowWayland = true
    }

    errorMsg?.let { msg ->
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text(msg, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    // Native init + asset sync can take a while; keep a plain black gap between system splash
    // and the shell / desktop (no loading spinner — matches desktopLaunchBlackout UX).
    if (!initialized) {
        Box(modifier = Modifier.fillMaxSize().background(Color.Black))
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

    Box(modifier = Modifier.fillMaxSize()) {
        Box(modifier = Modifier.fillMaxSize()) {
            ShellScreen(
                terminalFontKey = terminalFontKey,
                activeSessionId = activeTerminalSessionId,
                terminalSessionIds = terminalSessionIds,
                rendererSessionResetEpoch = rendererSessionResetEpoch,
                showKeyboardTrigger = if (showWayland) 0 else showKeyboardTrigger,
                onKeyboardTriggerConsumed = { showKeyboardTrigger = 0 },
                modifier = Modifier.fillMaxSize()
            )
            if (showWayland) {
                WaylandSurfaceView(
                    runtimeDir = waylandRuntimeDir,
                    mouseMode = mouseMode,
                    resolutionPercent = resolutionPercent,
                    scalePercent = scalePercent,
                    /* Keep false: if true, wl_buffer with NULL user_data is discarded (surface.c) when
                     * EGL Wayland import is off — many Mesa paths still use that buffer type. */
                    skipEglWaylandBind = false,
                    showKeyboardTrigger = showKeyboardTrigger,
                    keyboardWanted = keyboardWanted,
                    onKeyboardTriggerConsumed = { showKeyboardTrigger = 0 },
                    modifier = Modifier.fillMaxSize()
                )
            }
            if (desktopLaunchBlackout) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(Color.Black)
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
            desktopRendererLabel = desktopRendererMode,
            onDesktopRendererClick = {
                val idx = RENDERER_MODES.indexOf(desktopRendererMode)
                val next = RENDERER_MODES[(idx + 1) % RENDERER_MODES.size]
                persistDesktopRendererMode(next)
            },
            onViewClick = {
                menuOpen = false
                settingsOpen = true
            },
            onAppearanceClick = {
                menuOpen = false
                appearanceOpen = true
            },
            onSessionClick = {
                menuOpen = false
                sessionsDialogOpen = true
            },
            onKeyboardClick = {
                menuOpen = false
                keyboardWanted = true
                showKeyboardTrigger += 1
            },
            modifier = Modifier
                .fillMaxSize()
                .graphicsLayer { clip = false }
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
        if (appearanceOpen) {
            AppearanceDialog(
                terminalFontPrefKey = terminalFontKey,
                onTerminalFontPrefChange = { persistTerminalFont(it) },
                onDismiss = { appearanceOpen = false }
            )
        }
        if (displayScriptDialogOpen) {
            DisplayScriptDialog(
                initialScript = prefs.getString("display_startup_script", "") ?: "",
                onDismiss = { displayScriptDialogOpen = false },
                onConfirm = { script ->
                    prefs.edit().putString("display_startup_script", script.trimEnd()).apply()
                    displayScriptDialogOpen = false
                }
            )
        }
        if (sessionsDialogOpen) {
            SessionsDialog(
                sessionIds = terminalSessionIds,
                activeSessionId = activeTerminalSessionId,
                onSelectSession = { id ->
                    activeTerminalSessionId = id
                    sessionsDialogOpen = false
                },
                onAddSession = {
                    val next = (terminalSessionIds.maxOrNull() ?: 0) + 1
                    terminalSessionIds = terminalSessionIds + next
                    activeTerminalSessionId = next
                },
                onCloseSession = { id ->
                    if (terminalSessionIds.size > 1) {
                        val newList = terminalSessionIds.filter { it != id }
                        terminalSessionIds = newList
                        if (activeTerminalSessionId == id) {
                            activeTerminalSessionId = newList.first()
                        }
                    }
                },
                onDismiss = { sessionsDialogOpen = false }
            )
        }
    }
}
