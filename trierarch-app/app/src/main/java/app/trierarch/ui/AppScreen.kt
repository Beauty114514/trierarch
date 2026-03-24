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
import app.trierarch.ui.screens.SideMenu
import app.trierarch.ui.screens.TerminalScreen
import app.trierarch.ui.screens.ViewSettingsDialog
import app.trierarch.ui.screens.twoFingerSwipeFromLeft
import app.trierarch.wayland.WaylandSurfaceView
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun AppScreen() {
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
    var waylandOn by remember { mutableStateOf(false) }
    var showWayland by remember { mutableStateOf(false) }
    var settingsOpen by remember { mutableStateOf(false) }
    var displayScriptDialogOpen by remember { mutableStateOf(false) }
    var mouseMode by remember { mutableStateOf(MOUSE_MODE_TOUCHPAD) }
    var resolutionPercent by remember { mutableStateOf(100) }
    var scalePercent by remember { mutableStateOf(100) }
    var showKeyboardTrigger by remember { mutableStateOf(0) }
    val prefs = remember(context) { context.getSharedPreferences("trierarch_prefs", 0) }

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

    LaunchedEffect(prootSpawned, waylandOn) {
        if (!prootSpawned) return@LaunchedEffect
        if (waylandOn) {
            val keymapTarget = File(waylandRuntimeDir, "keymap_us.xkb")
            if (!keymapTarget.exists()) {
                context.assets.open("keymap_us.xkb").use { input ->
                    keymapTarget.outputStream().use { out ->
                        input.copyTo(out)
                    }
                }
            }
            WaylandBridge.nativeStartServer(waylandRuntimeDir)
        } else {
            WaylandBridge.nativeStopWayland()
            showWayland = false
        }
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
            text = { Text("Restart the app to start the Arch terminal.") },
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
            Text(AppStrings.STARTING_ARCH, color = MaterialTheme.colorScheme.onBackground)
        }
        return
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Box(modifier = Modifier.fillMaxSize().twoFingerSwipeFromLeft(onSwipeRight = { menuOpen = true })) {
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
                    }
                )
            }
        }
        SideMenu(
            visible = menuOpen,
            onDismiss = { menuOpen = false },
            waylandOn = waylandOn,
            onWaylandClick = { waylandOn = !waylandOn },
            onDisplayClick = {
                if (showWayland) {
                    showWayland = false
                } else {
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
                    showWayland = true
                }
                menuOpen = false
            },
            onDisplayLongPress = { displayScriptDialogOpen = true },
            onViewClick = { settingsOpen = true },
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
