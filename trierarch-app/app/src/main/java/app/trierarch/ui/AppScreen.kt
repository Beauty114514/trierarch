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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalContext
import app.trierarch.NativeBridge
import app.trierarch.PulseAssets
import app.trierarch.ProgressCallback
import app.trierarch.TerminalSessionIds
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
     * - proot running: first TerminalView layout runs RustPtySession.updateSize (forkpty winsize matches the grid)
     * - Wayland server running: WaylandBridge.nativeStartServer(runtimeDir)
     * - view: `showWayland` toggles desktop on top; **ShellScreen stays composed** underneath so
     *   [TerminalView] / per-session [TerminalEmulator] buffers survive Display ↔ terminal switches.
     *
     * Invariants / ordering constraints:
     * - proot must be spawned before starting the Wayland server because Wayland clients live inside
     *   the proot environment and rely on the socket under XDG_RUNTIME_DIR.
     * - Display startup script is injected via PTY stdin on **session 0** (headless shell), not the
     *   visible terminal tabs (1+). It is guarded by nativeHasActiveClients() so we don’t re-run the
     *   script once a desktop client is already connected.
     *
     * Entry modes:
     * - Default launcher entry: may auto-run Display script once, then switch to Wayland view.
     * - Terminal shortcut entry (`startInTerminal=true`): stays in terminal, never auto-runs Display.
     *
     * Menu: [FloatingMenuOrb] (tap) opens a frosted panel centered on screen; orb stays draggable.
     */
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

    fun ensureAudioDefaultSinkSnippet(script: String): String {
        val s = script.trimEnd()
        if (s.isEmpty()) return s
        val marker = "# trierarch:audio-default-sink"
        if (s.contains(marker)) return s

        // Why: the host pulse daemon may default to a null sink; many apps "play" happily but users
        // hear nothing unless the default sink is set to the real output.
        val snippet = """
            
            $marker
            # Ensure desktop apps use the real output (AAudio) instead of the null sink.
            for i in {1..50}; do
              pactl info >/dev/null 2>&1 && break
              sleep 0.1
            done
            pactl set-default-sink trierarch-out >/dev/null 2>&1 || true
        """.trimIndent()

        return s + "\n" + snippet + "\n"
    }

    fun runDisplayStartupScriptIfNeeded() {
        val hasClients = try {
            WaylandBridge.nativeHasActiveClients()
        } catch (_: Throwable) {
            false
        }
        if (!hasClients) {
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
        withContext(Dispatchers.IO) {
            PulseAssets.syncFromAssetsIfNeeded(context)
        }
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
            val script = prefs.getString("display_startup_script", "")?.trim()
            if (!script.isNullOrEmpty()) {
                runDisplayStartupScriptIfNeeded()
            }
        }
        pendingAutoShowWayland = true
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

    Box(modifier = Modifier.fillMaxSize()) {
        Box(modifier = Modifier.fillMaxSize()) {
            ShellScreen(
                terminalFontKey = terminalFontKey,
                activeSessionId = activeTerminalSessionId,
                terminalSessionIds = terminalSessionIds,
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
                    val patched = ensureAudioDefaultSinkSnippet(script)
                    prefs.edit().putString("display_startup_script", patched).apply()
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
