package app.trierarch.ui

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Handler
import androidx.activity.ComponentActivity
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import android.os.Looper
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.zIndex
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import app.trierarch.NativeBridge
import app.trierarch.PtyOutputRelay
import app.trierarch.X11Runtime
import app.trierarch.TerminalSessionIds
import app.trierarch.WaylandBridge
import app.trierarch.wayland.input.InputRouteState
import app.trierarch.shell.ShellFonts
import app.trierarch.ui.dialog.MOUSE_MODE_TABLET
import app.trierarch.ui.dialog.MOUSE_MODE_TOUCHPAD
import app.trierarch.ui.drawer.AppDrawer
import app.trierarch.ui.drawer.pages.DrawerPagedHost
import app.trierarch.ui.drawer.pages.ArchDrawerPage
import app.trierarch.ui.containers.ContainersScreen
import app.trierarch.ui.drawer.pages.DebianDrawerPage
import app.trierarch.ui.drawer.pages.WineDrawerPage
import app.trierarch.ui.prefs.AppPrefs
import app.trierarch.ui.runtime.DisplayOrchestrator
import app.trierarch.ui.runtime.GraphicsModeController
import app.trierarch.ui.runtime.MandatoryStartupStoragePermissionsEffect
import app.trierarch.ui.runtime.NativeInstallCoordinator
import app.trierarch.ui.runtime.StartupManageExternalPermissions
import app.trierarch.ui.runtime.StartupStoragePermissions
import app.trierarch.ui.runtime.TerminalSessionController
import app.trierarch.ui.runtime.WaylandVisibilityCoordinator
import androidx.compose.foundation.clickable
import androidx.compose.ui.input.pointer.pointerInput
import app.trierarch.ui.orb.FloatingMenuOrb
import app.trierarch.ui.setup.InstallScreen
import app.trierarch.ui.shell.ShellScreen
import app.trierarch.wayland.WaylandSurfaceView
import app.trierarch.ui.x11.EmbeddedX11Surface
import com.winlator.EmbeddedWinlatorController
import com.termux.x11.EmbeddedX11Controller
import com.termux.x11.X11OutputSettings
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import androidx.compose.material3.rememberDrawerState
import java.util.regex.Pattern

private val VULKAN_MODES = listOf("LLVMPIPE", "VENUS")
private val OPENGL_MODES = listOf("LLVMPIPE", "VIRGL")

private const val X11_MODE_LABEL_NATIVE = "Native"
private const val X11_MODE_LABEL_SCALED = "Scaled"
private const val X11_MODE_LABEL_EXACT = "Fixed size"
private const val X11_MODE_LABEL_CUSTOM = "Custom"

private fun x11ResolutionModeLabelForInternal(mode: String): String = when (mode) {
    "native" -> X11_MODE_LABEL_NATIVE
    "scaled" -> X11_MODE_LABEL_SCALED
    "exact" -> X11_MODE_LABEL_EXACT
    "custom" -> X11_MODE_LABEL_CUSTOM
    else -> X11_MODE_LABEL_NATIVE
}

private fun x11ResolutionModeInternalForLabel(label: String): String = when (label) {
    X11_MODE_LABEL_NATIVE -> "native"
    X11_MODE_LABEL_SCALED -> "scaled"
    X11_MODE_LABEL_EXACT -> "exact"
    X11_MODE_LABEL_CUSTOM -> "custom"
    else -> "native"
}

private val X11_CUSTOM_RESOLUTION_PATTERN: Pattern = Pattern.compile("^\\s*(\\d{2,4})\\s*x\\s*(\\d{2,4})\\s*\$")

private enum class UiMode { TERMINAL, ARCH_WAYLAND_DESKTOP, DEBIAN_X11_DESKTOP, CONTAINERS, WINLATOR }

@Composable
fun AppScreen(startInTerminal: Boolean = false) {
    // Order: native init → rootfs → proot before Wayland server. Shell UI stays in the tree
    // when toggling desktop so terminal buffers survive. Headless injectors use per-namespace slot 0
    // ([TerminalSessionIds]); renderer changes clear PTYs (env is fixed at spawn).
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var initialized by remember { mutableStateOf(false) }
    var hasArchRootfs by remember { mutableStateOf(false) }
    var hasDebianRootfs by remember { mutableStateOf(false) }
    var hasWineRootfs by remember { mutableStateOf(false) }
    var installProgress by remember { mutableStateOf(0 to "Preparing...") }
    var installDone by remember { mutableStateOf(false) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var downloadStarted by remember { mutableStateOf(false) }
    var menuOpen by remember { mutableStateOf(false) }
    var uiMode by remember(startInTerminal) {
        // Always start in Terminal after initialization. Users can still switch modes from the drawer
        // (orb menu is legacy; kept for parity while migrating).
        mutableStateOf(UiMode.TERMINAL)
    }
    var waylandVisible by remember { mutableStateOf(false) }
    val showWayland = waylandVisible && uiMode == UiMode.ARCH_WAYLAND_DESKTOP
    val showX11 = uiMode == UiMode.DEBIAN_X11_DESKTOP
    val showWinlator = uiMode == UiMode.WINLATOR
    var winlatorContainerId by remember { mutableIntStateOf(-1) }
    var winlatorController by remember { mutableStateOf<EmbeddedWinlatorController?>(null) }
    var settingsOpen by remember { mutableStateOf(false) }
    var waylandScriptEditorOpen by remember { mutableStateOf(false) }
    var x11ScriptEditorOpen by remember { mutableStateOf(false) }
    var terminalSessionState by remember { mutableStateOf(TerminalSessionController.initialState()) }
    var mouseMode by remember { mutableStateOf(MOUSE_MODE_TOUCHPAD) }
    var resolutionPercent by remember { mutableStateOf(100) }
    var scalePercent by remember { mutableStateOf(100) }
    var showKeyboardTrigger by remember { mutableStateOf(0) }
    var keyboardWanted by remember { mutableStateOf(false) }
    // Keep the preference (launcherDefault) but do not auto-enter display modes after init.
    var pendingAutoShowWayland by remember { mutableStateOf(false) }
    var desktopVulkanMode by remember { mutableStateOf("LLVMPIPE") }
    var desktopOpenGLMode by remember { mutableStateOf("LLVMPIPE") }
    var desktopHiddenInjectedKey by remember { mutableStateOf("") }
    /** Incremented when graphics modes change so [ShellScreen] drops cached terminal sessions. */
    var rendererSessionResetEpoch by remember { mutableIntStateOf(0) }
    /** Full-screen cover during default desktop launch or Display toggle so the terminal flash is hidden. */
    var desktopLaunchBlackout by remember(startInTerminal) {
        mutableStateOf(!startInTerminal)
    }
    var terminalFontKey by remember { mutableStateOf(ShellFonts.DEFAULT_ID) }
    val prefs = remember(context) { context.getSharedPreferences("trierarch_prefs", 0) }
    var storagePermissionOk by remember(context) {
        mutableStateOf(StartupStoragePermissions.allGranted(context))
    }
    MandatoryStartupStoragePermissionsEffect(
        onMandatoryGranted = { granted -> storagePermissionOk = granted },
    )
    /**
     * Optional “All files access” step during startup only ([initialized] still false).
     * Cleared after skip, return from settings, leaving app, or when access is already granted.
     */
    var optionalManageExternalInitHandled by remember { mutableStateOf(false) }
    var expectingReturnFromManageAllFilesSettings by remember { mutableStateOf(false) }

    DisposableEffect(context) {
        val activity = context as? ComponentActivity
        if (activity == null) {
            return@DisposableEffect onDispose { }
        }
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> {
                    if (StartupManageExternalPermissions.isGranted(context)) {
                        optionalManageExternalInitHandled = true
                    }
                    if (expectingReturnFromManageAllFilesSettings) {
                        expectingReturnFromManageAllFilesSettings = false
                        optionalManageExternalInitHandled = true
                    }
                }
                Lifecycle.Event.ON_STOP -> {
                    if (activity.isChangingConfigurations) return@LifecycleEventObserver
                    if (initialized) return@LifecycleEventObserver
                    if (!storagePermissionOk) return@LifecycleEventObserver
                    if (optionalManageExternalInitHandled) return@LifecycleEventObserver
                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return@LifecycleEventObserver
                    if (StartupManageExternalPermissions.isGranted(context)) return@LifecycleEventObserver
                    if (expectingReturnFromManageAllFilesSettings) return@LifecycleEventObserver
                    optionalManageExternalInitHandled = true
                }
                else -> {}
            }
        }
        activity.lifecycle.addObserver(observer)
        onDispose { activity.lifecycle.removeObserver(observer) }
    }

    var x11MouseMode by remember { mutableStateOf(EmbeddedX11Controller.MouseMode.TOUCHPAD) }
    var x11ResolutionModeLabel by remember { mutableStateOf(X11_MODE_LABEL_NATIVE) }
    var x11DisplayScale by remember { mutableStateOf(100) }
    var x11ResolutionExact by remember { mutableStateOf("1280x1024") }
    var x11ResolutionCustom by remember { mutableStateOf("1280x1024") }
    val headlessX11InjectHandler = remember { Handler(Looper.getMainLooper()) }
    val drawerState = rememberDrawerState(initialValue = DrawerValue.Closed)
    val waylandRuntimeDir = remember(context) {
        File(context.filesDir, "usr/tmp").apply { mkdirs() }.absolutePath
    }
    val desktopSocketName = "wayland-trierarch-desktop"
    var desktopServerId by remember { mutableStateOf(0L) }
    var launcherDefault by remember {
        mutableStateOf(AppPrefs.readLauncherDefault(prefs))
    }

    fun enterTerminal() {
        try { WaylandBridge.nativeSetWmMode(WaylandBridge.WM_MODE_NESTED) } catch (_: Throwable) {}
        uiMode = UiMode.TERMINAL
        waylandVisible = false
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.LEGACY_ARCH_X11_PTY, false)
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.ARCH_WAYLAND_DISPLAY, false)
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.DEBIAN_X11_DISPLAY, false)
        desktopLaunchBlackout = false
        menuOpen = false
    }

    fun enterArchWaylandDesktop() {
        if (!DisplayOrchestrator.prepareWaylandRuntimeAndStartServer(context, waylandRuntimeDir)) {
            menuOpen = false
            return
        }
        if (desktopServerId == 0L) {
            desktopServerId = try {
                WaylandBridge.nativeCreateServer(waylandRuntimeDir, desktopSocketName)
            } catch (_: Throwable) { 0L }
        }
        if (desktopServerId != 0L) {
            try { WaylandBridge.nativeSetActiveServer(desktopServerId) } catch (_: Throwable) {}
        }
        try { WaylandBridge.nativeSetWmMode(WaylandBridge.WM_MODE_NESTED) } catch (_: Throwable) {}
        desktopLaunchBlackout = true
        desktopHiddenInjectedKey = DisplayOrchestrator.runArchWaylandStartupScriptIfNeeded(
            prefs = prefs,
            desktopSocketName = desktopSocketName,
            vulkanMode = desktopVulkanMode,
            openGLMode = desktopOpenGLMode,
            currentHiddenInjectedKey = desktopHiddenInjectedKey,
        ).hiddenInjectedKey
        pendingAutoShowWayland = true
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.LEGACY_ARCH_X11_PTY, false)
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.ARCH_WAYLAND_DISPLAY, false)
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.DEBIAN_X11_DISPLAY, false)
        uiMode = UiMode.ARCH_WAYLAND_DESKTOP
        waylandVisible = false
        menuOpen = false
    }

    fun enterDebianDesktop() {
        X11Runtime.ensureX11ServerProcessStarted(context)
        DisplayOrchestrator.runDebianX11DesktopStartupScript(
            context = context,
            prefs = prefs,
            headlessInjectHandler = headlessX11InjectHandler,
            hasDebianRootfs = hasDebianRootfs,
        )
        menuOpen = false
        desktopLaunchBlackout = false
        PtyOutputRelay.setSessionIoLoggingEnabled(TerminalSessionIds.DEBIAN_X11_DISPLAY, false)
        waylandVisible = false
        pendingAutoShowWayland = false
        uiMode = UiMode.DEBIAN_X11_DESKTOP
    }

    fun cycleLauncherDefault() {
        launcherDefault = AppPrefs.cycleLauncherDefaultPref(launcherDefault)
        AppPrefs.writeLauncherDefault(prefs, launcherDefault)
    }

    fun setLauncherDefaultFromMenuLabel(menuLabel: String) {
        launcherDefault = AppPrefs.menuLabelToLauncherPref(menuLabel)
        AppPrefs.writeLauncherDefault(prefs, launcherDefault)
    }

    LaunchedEffect(startInTerminal) {
        if (startInTerminal) {
            uiMode = UiMode.TERMINAL
            pendingAutoShowWayland = false
            desktopLaunchBlackout = false
        }
    }

    LaunchedEffect(Unit) {
        launcherDefault = AppPrefs.readLauncherDefault(prefs)
        mouseMode = AppPrefs.readInt(prefs, "mouse_mode", MOUSE_MODE_TOUCHPAD)
        resolutionPercent = AppPrefs.readInt(prefs, "resolution_percent", 100).coerceIn(10, 100)
        scalePercent = AppPrefs.readInt(prefs, "scale_percent", 100)
            .coerceIn(100, 1000)
            .let { v -> ((v + 50) / 100) * 100 }
        terminalFontKey = AppPrefs.readString(prefs, ShellFonts.PREF_KEY, ShellFonts.DEFAULT_ID)
        x11MouseMode = run {
            val raw = AppPrefs.readString(prefs, "x11_mouse_mode", "touchpad").trim().lowercase()
            when {
                raw == "touch" -> EmbeddedX11Controller.MouseMode.TOUCH
                raw == "touchpad" -> EmbeddedX11Controller.MouseMode.TOUCHPAD
                raw.contains("touch") && !raw.contains("pad") -> EmbeddedX11Controller.MouseMode.TOUCH
                else -> EmbeddedX11Controller.MouseMode.TOUCHPAD
            }
        }
        x11ResolutionModeLabel = x11ResolutionModeLabelForInternal(
            X11OutputSettings.getResolutionMode(context.applicationContext).trim().lowercase(),
        )
        x11DisplayScale = X11OutputSettings.getDisplayScalePercent(context.applicationContext)
            .coerceIn(30, 300)
            .let { v -> (v / 10) * 10 }
        x11ResolutionExact = X11OutputSettings.getResolutionExact(context.applicationContext)
        x11ResolutionCustom = X11OutputSettings.getResolutionCustom(context.applicationContext)
    }

    fun setDesktopVulkanMode(mode: String) {
        val prev = GraphicsModeController.Modes(desktopVulkanMode, desktopOpenGLMode)
        val next = GraphicsModeController.sanitize(
            GraphicsModeController.Modes(vulkan = mode, openGL = desktopOpenGLMode),
            allowedVulkan = VULKAN_MODES,
            allowedOpenGL = OPENGL_MODES,
        )
        if (GraphicsModeController.applyAndMaybeToggleVirglHost(prefs, prev, next)) {
            rendererSessionResetEpoch++
        }
        desktopVulkanMode = next.vulkan
        desktopOpenGLMode = next.openGL
    }

    fun setDesktopOpenGLMode(mode: String) {
        val prev = GraphicsModeController.Modes(desktopVulkanMode, desktopOpenGLMode)
        val next = GraphicsModeController.sanitize(
            GraphicsModeController.Modes(vulkan = desktopVulkanMode, openGL = mode),
            allowedVulkan = VULKAN_MODES,
            allowedOpenGL = OPENGL_MODES,
        )
        if (GraphicsModeController.applyAndMaybeToggleVirglHost(prefs, prev, next)) {
            rendererSessionResetEpoch++
        }
        desktopVulkanMode = next.vulkan
        desktopOpenGLMode = next.openGL
    }

    fun persistMouseMode(mode: Int) {
        mouseMode = mode
        AppPrefs.writeInt(prefs, "mouse_mode", mode)
    }

    fun persistResolutionPercent(pct: Int) {
        resolutionPercent = pct.coerceIn(10, 100)
        AppPrefs.writeInt(prefs, "resolution_percent", resolutionPercent)
    }

    fun persistScalePercent(pct: Int) {
        scalePercent = pct.coerceIn(100, 1000).let { v -> ((v + 50) / 100) * 100 }
        AppPrefs.writeInt(prefs, "scale_percent", scalePercent)
    }

    fun persistTerminalFont(key: String) {
        terminalFontKey = key
        AppPrefs.writeString(prefs, ShellFonts.PREF_KEY, key)
    }

    fun persistX11MouseMode(mode: EmbeddedX11Controller.MouseMode) {
        x11MouseMode = mode
        AppPrefs.writeString(
            prefs,
            "x11_mouse_mode",
            if (mode == EmbeddedX11Controller.MouseMode.TOUCH) "touch" else "touchpad",
        )
    }

    fun applyX11ResolutionModeFromLabel(label: String) {
        x11ResolutionModeLabel = label
        val internal = x11ResolutionModeInternalForLabel(label)
        X11OutputSettings.setResolutionMode(context.applicationContext, internal)
    }

    fun applyX11DisplayScaleFromLabel(label: String) {
        val pct = label.removeSuffix("%").trim().toIntOrNull() ?: return
        x11DisplayScale = pct.coerceIn(30, 300).let { v -> (v / 10) * 10 }
        X11OutputSettings.setDisplayScalePercent(context.applicationContext, x11DisplayScale)
    }

    fun applyX11ResolutionExactFromLabel(label: String) {
        if (label.isBlank()) return
        x11ResolutionExact = label
        X11OutputSettings.setResolutionExact(context.applicationContext, label)
    }

    fun applyX11ResolutionCustomWxh() {
        val raw = x11ResolutionCustom.trim()
        val m = X11_CUSTOM_RESOLUTION_PATTERN.matcher(raw)
        if (!m.matches()) {
            return
        }
        val wxh = "${m.group(1)}x${m.group(2)}"
        x11ResolutionCustom = wxh
        X11OutputSettings.setResolutionCustom(context.applicationContext, wxh)
    }

    LaunchedEffect(storagePermissionOk, optionalManageExternalInitHandled) {
        if (!storagePermissionOk) return@LaunchedEffect
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            !StartupManageExternalPermissions.isGranted(context) &&
            !optionalManageExternalInitHandled
        ) {
            return@LaunchedEffect
        }

        val r = NativeInstallCoordinator.initNativeAndSyncAssets(
            context = context,
            prefs = prefs,
            allowedVulkan = VULKAN_MODES,
            allowedOpenGL = OPENGL_MODES,
        )
        if (!r.ok) {
            errorMsg = "Failed to initialize native layer"
            return@LaunchedEffect
        }

        initialized = true
        hasArchRootfs = r.hasArchRootfs
        hasDebianRootfs = r.hasDebianRootfs
        hasWineRootfs = r.hasWineRootfs

        desktopVulkanMode = r.desktopModes.vulkan
        desktopOpenGLMode = r.desktopModes.openGL
        // Apply persisted graphics modes to native layer (affects newly spawned proot processes).
        GraphicsModeController.applyAndMaybeToggleVirglHost(
            prefs = prefs,
            previous = GraphicsModeController.Modes(desktopVulkanMode, desktopOpenGLMode),
            modes = GraphicsModeController.Modes(desktopVulkanMode, desktopOpenGLMode),
        )
    }

    LaunchedEffect(initialized, hasArchRootfs, hasDebianRootfs, hasWineRootfs) {
        if (!initialized) return@LaunchedEffect
        if ((!hasArchRootfs || !hasDebianRootfs || !hasWineRootfs) && !installDone && !downloadStarted) {
            downloadStarted = true
            scope.launch {
                val res = NativeInstallCoordinator.downloadMissingRootfsSequentially(context) { pct, msg ->
                    installProgress = pct to msg
                }
                withContext(Dispatchers.Main) {
                    hasArchRootfs = res.hasArchRootfs
                    hasDebianRootfs = res.hasDebianRootfs
                    hasWineRootfs = res.hasWineRootfs
                    if (res.allOk) {
                        installDone = true
                        downloadStarted = false
                    } else {
                        errorMsg = when {
                            !res.archOk -> "Arch rootfs download failed"
                            !res.debianOk -> "Debian rootfs download failed"
                            !res.wineOk -> "Wine environment setup failed"
                            else -> "Setup failed"
                        }
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

    LaunchedEffect(showX11) {
        // Keep shell hardware key routing in Trierarch while X11 surface is on top.
        InputRouteState.lorieX11DisplayVisible = showX11
        if (showX11) {
            // Avoid two native full-screen surfaces at once; X11 and Wayland are mutually exclusive.
            waylandVisible = false
            pendingAutoShowWayland = false
        }
    }

    LaunchedEffect(menuOpen, settingsOpen, waylandScriptEditorOpen, x11ScriptEditorOpen) {
        // Don't auto-pop the IME while the user is interacting with menus/dialogs.
        if (menuOpen || settingsOpen || waylandScriptEditorOpen || x11ScriptEditorOpen) {
            keyboardWanted = false
        }
    }

    LaunchedEffect(pendingAutoShowWayland) {
        if (!pendingAutoShowWayland) return@LaunchedEffect
        WaylandVisibilityCoordinator.waitUntilDesktopClientReady(
            isStillPending = { pendingAutoShowWayland && uiMode == UiMode.ARCH_WAYLAND_DESKTOP && !waylandVisible },
            hasActiveClients = { WaylandBridge.nativeHasActiveClients() },
            onReady = {
                waylandVisible = true
                pendingAutoShowWayland = false
            },
        )
    }

    LaunchedEffect(hasArchRootfs, hasDebianRootfs, hasWineRootfs, startInTerminal) {
        if (!hasArchRootfs || !hasDebianRootfs || !hasWineRootfs) return@LaunchedEffect
        var waited = 0
        while (!NativeBridge.isSessionAlive(TerminalSessionIds.ARCH_TERMINAL) && waited < 256) {
            delay(32)
            waited++
        }
        if (!NativeBridge.isSessionAlive(TerminalSessionIds.ARCH_TERMINAL)) {
            desktopLaunchBlackout = false
            return@LaunchedEffect
        }

        desktopLaunchBlackout = false
        if (uiMode == UiMode.ARCH_WAYLAND_DESKTOP) {
            return@LaunchedEffect
        }
        // Default after init: terminal; user picks desktop from the drawer.
        uiMode = UiMode.TERMINAL
    }

    errorMsg?.let { msg ->
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text(msg, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    if (!storagePermissionOk) {
        Box(modifier = Modifier.fillMaxSize().background(Color.Black))
        return
    }

    val waitingOptionalManageExternal =
        storagePermissionOk &&
            !initialized &&
            !optionalManageExternalInitHandled &&
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            !StartupManageExternalPermissions.isGranted(context)

    // Native init + asset sync can take a while; keep a plain black gap between system splash
    // and the shell / desktop (no loading spinner — matches desktopLaunchBlackout UX).
    if (!initialized) {
        Box(modifier = Modifier.fillMaxSize().background(Color.Black))
        if (waitingOptionalManageExternal) {
            AlertDialog(
                onDismissRequest = { optionalManageExternalInitHandled = true },
                properties = DialogProperties(dismissOnClickOutside = false),
                title = { Text("All files access (recommended)") },
                text = {
                    Text(
                        "Granting All files access can improve access to shared storage paths used by some features. " +
                            "This is optional—you can continue without granting and use the app normally.",
                    )
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            expectingReturnFromManageAllFilesSettings = true
                            StartupManageExternalPermissions.openAllFilesAccessSettings(context)
                        },
                    ) {
                        Text("Open settings")
                    }
                },
                dismissButton = {
                    TextButton(onClick = { optionalManageExternalInitHandled = true }) {
                        Text("Continue without")
                    }
                },
            )
        }
        return
    }

    if ((!hasArchRootfs || !hasDebianRootfs || !hasWineRootfs) && !installDone) {
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

    AppDrawer(
        drawerState = drawerState,
        drawerContent = {
            DrawerPagedHost(
                archContent = {
                    ArchDrawerPage(
                        prefs = prefs,
                        drawerState = drawerState,
                        scope = scope,
                        terminalFontKey = terminalFontKey,
                        terminalSessionState = terminalSessionState,
                        launcherDefault = launcherDefault,
                        desktopVulkanMode = desktopVulkanMode,
                        desktopOpenGLMode = desktopOpenGLMode,
                        mouseMode = mouseMode,
                        resolutionPercent = resolutionPercent,
                        scalePercent = scalePercent,
                        waylandScriptEditorOpen = waylandScriptEditorOpen,
                        onWaylandScriptEditorOpenChange = { waylandScriptEditorOpen = it },
                        onEnterWaylandDesktop = { enterArchWaylandDesktop() },
                        onEnterTerminal = { enterTerminal() },
                        onLauncherDefaultSelect = { setLauncherDefaultFromMenuLabel(it) },
                        onDesktopVulkanSelect = { setDesktopVulkanMode(it) },
                        onDesktopOpenGLSelect = { setDesktopOpenGLMode(it) },
                        onTerminalFontSelectLabel = { label ->
                            val id = ShellFonts.options.find { it.label == label }?.id ?: ShellFonts.DEFAULT_ID
                            persistTerminalFont(id)
                        },
                        onTerminalSessionStateChange = { terminalSessionState = it },
                        onMouseModeSelectLabel = { label ->
                            persistMouseMode(if (label == "Tablet") MOUSE_MODE_TABLET else MOUSE_MODE_TOUCHPAD)
                        },
                        onResolutionPercentSelectLabel = { label ->
                            val pct = label.removeSuffix("%").trim().toIntOrNull()
                            if (pct != null) persistResolutionPercent(pct)
                        },
                        onScalePercentSelectLabel = { label ->
                            val pct = label.removeSuffix("%").trim().toIntOrNull()
                            if (pct != null) persistScalePercent(pct)
                        },
                        vulkanOptions = VULKAN_MODES,
                        openGLOptions = OPENGL_MODES,
                    )
                },
                wineContent = {
                    WineDrawerPage(
                        onOpenContainers = {
                            scope.launch {
                                uiMode = UiMode.CONTAINERS
                                drawerState.close()
                            }
                        },
                    )
                },
                debianContent = {
                    DebianDrawerPage(
                        prefs = prefs,
                        drawerState = drawerState,
                        scope = scope,
                        terminalSessionState = terminalSessionState,
                        onTerminalSessionStateChange = { terminalSessionState = it },
                        x11MouseModeLabel = if (x11MouseMode == EmbeddedX11Controller.MouseMode.TOUCH) "Touch" else "Touchpad",
                        onX11MouseModeSelectLabel = {
                            persistX11MouseMode(
                                if (it == "Touch") EmbeddedX11Controller.MouseMode.TOUCH else EmbeddedX11Controller.MouseMode.TOUCHPAD
                            )
                        },
                        x11ResolutionModeLabel = x11ResolutionModeLabel,
                        onX11ResolutionModeSelectLabel = { applyX11ResolutionModeFromLabel(it) },
                        x11DisplayScaleLabel = "${x11DisplayScale}%",
                        onX11DisplayScaleSelectLabel = { applyX11DisplayScaleFromLabel(it) },
                        x11ResolutionExactLabel = x11ResolutionExact,
                        onX11ResolutionExactSelectLabel = { applyX11ResolutionExactFromLabel(it) },
                        x11ResolutionCustom = x11ResolutionCustom,
                        onX11ResolutionCustomChange = { x11ResolutionCustom = it },
                        onX11ResolutionCustomApply = { applyX11ResolutionCustomWxh() },
                        x11ScriptEditorOpen = x11ScriptEditorOpen,
                        onX11ScriptEditorOpenChange = { x11ScriptEditorOpen = it },
                        onEnterDebianDesktop = { enterDebianDesktop() },
                        onEnterTerminal = { enterTerminal() },
                        onExitDisplayModes = {
                            uiMode = UiMode.TERMINAL
                            waylandVisible = false
                            pendingAutoShowWayland = false
                        },
                    )
                },
            )
        },
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            if (uiMode == UiMode.CONTAINERS) {
                ContainersScreen(
                    modifier = Modifier
                        .fillMaxSize()
                        .zIndex(0f),
                    onRunContainer = { id ->
                        winlatorContainerId = id
                        uiMode = UiMode.WINLATOR
                    },
                )
            }
            else {
                // Shell (PTY) below; Wayland/X11 surfaces above when enabled.
                ShellScreen(
                    terminalFontKey = terminalFontKey,
                    activeSessionId = terminalSessionState.activeSessionId,
                    terminalSessionIds = terminalSessionState.sessionIds,
                    rendererSessionResetEpoch = rendererSessionResetEpoch,
                    showKeyboardTrigger = if (showWayland) 0 else showKeyboardTrigger,
                    onKeyboardTriggerConsumed = { showKeyboardTrigger = 0 },
                    modifier = Modifier
                        .fillMaxSize()
                        .zIndex(0f),
                )
            }
            if (desktopLaunchBlackout && uiMode != UiMode.CONTAINERS) {
                // Dark cover while Wayland display session starts (Lorie is not in this window).
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .zIndex(1f)
                        .background(Color.Black),
                )
            }
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
                    modifier = Modifier
                        .fillMaxSize()
                        .zIndex(1.5f),
                )
            }
            if (showX11) {
                EmbeddedX11Surface(
                    visible = true,
                    mouseMode = x11MouseMode,
                    modifier = Modifier
                        .fillMaxSize()
                        .zIndex(1.6f),
                )
            }
            if (showWinlator && winlatorContainerId > 0) {
                AndroidView(
                    factory = { ctx ->
                        val activity = ctx as? Activity
                            ?: error("Embedded Winlator requires Activity context")
                        val host = androidx.appcompat.widget.ContentFrameLayout(ctx)
                        val controller = EmbeddedWinlatorController(activity, host)
                        controller.start(winlatorContainerId)
                        winlatorController = controller
                        host
                    },
                    modifier = Modifier
                        .fillMaxSize()
                        .zIndex(1.7f),
                )
            }
        }
        FloatingMenuOrb(
            prefs = prefs,
            onClick = {
                scope.launch {
                    if (drawerState.isOpen) drawerState.close() else drawerState.open()
                }
            },
            modifier = Modifier
                .fillMaxSize()
                .graphicsLayer { clip = false }
        )
        // Script editors now live in the drawer (Compose), no glass dialog.
    }

    LaunchedEffect(uiMode) {
        if (uiMode != UiMode.WINLATOR) {
            winlatorController?.stop()
            winlatorController = null
            winlatorContainerId = -1
        }
    }
}
