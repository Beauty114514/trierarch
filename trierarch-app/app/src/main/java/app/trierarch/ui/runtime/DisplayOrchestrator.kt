package app.trierarch.ui.runtime

import android.content.Context
import android.os.Handler
import app.trierarch.NativeBridge
import app.trierarch.TerminalSessionIds
import app.trierarch.WaylandBridge
import app.trierarch.ui.prefs.AppPrefs
import java.io.File

object DisplayOrchestrator {
    private const val HEADLESS_X11_INJECT_DELAY_MS = 400L
    private const val X11_SOCKET_WAIT_POLL_MS = 120L
    private const val X11_SOCKET_WAIT_MAX_POLLS = 120 // ~14s

    data class WaylandEnvState(
        val hiddenInjectedKey: String,
    )

    /**
     * Copy bundled keymap (if missing) and start the in-process Wayland compositor socket + dispatch thread.
     * Idempotent with respect to native [WaylandBridge.nativeStartServer] (safe to call more than once).
     */
    fun prepareWaylandRuntimeAndStartServer(context: Context, waylandRuntimeDir: String): Boolean {
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

    fun ensureArchWaylandDisplaySession() {
        if (!NativeBridge.isSessionAlive(TerminalSessionIds.ARCH_WAYLAND_DISPLAY)) {
            NativeBridge.spawnSession(TerminalSessionIds.ARCH_WAYLAND_DISPLAY, 24, 80)
        }
    }

    /**
     * Headless [TerminalSessionIds.DEBIAN_X11_DISPLAY] (slot 0) — only for inject when no interactive Debian
     * tab has been used yet.
     */
    fun ensureDebianX11DisplaySession(hasDebianRootfs: Boolean): Boolean {
        if (!hasDebianRootfs) return false
        if (NativeBridge.isSessionAlive(TerminalSessionIds.DEBIAN_X11_DISPLAY)) return true
        return NativeBridge.spawnSessionInRootfs(
            TerminalSessionIds.DEBIAN_X11_DISPLAY,
            24,
            80,
            TerminalSessionIds.rootfsKindForNativeId(TerminalSessionIds.DEBIAN_X11_DISPLAY),
        )
    }

    /**
     * Implicit DISPLAY/XDG plus the user’s script (prefs).
     * Waits for X0 unix socket before injecting to reduce "desktop started too early" failures.
     */
    fun runDebianX11DesktopStartupScript(
        context: Context,
        prefs: android.content.SharedPreferences,
        headlessInjectHandler: Handler,
        hasDebianRootfs: Boolean,
    ) {
        if (!ensureDebianX11DisplaySession(hasDebianRootfs)) return
        val targetId = TerminalSessionIds.DEBIAN_X11_DISPLAY
        val user = AppPrefs.readDebianDesktopStartupScript(prefs).trim()
        val payload = buildString {
            append(AppPrefs.buildDebianX11ImplicitEnvSnippet())
            if (user.isNotEmpty()) {
                append(user)
                if (!user.endsWith("\n")) append("\n")
            }
        }
        if (payload.isEmpty()) return
        val bytes = payload.toByteArray(Charsets.UTF_8)

        val x0 = File(context.filesDir, "tmp/.X11-unix/X0")
        var polls = 0
        val inject = {
            headlessInjectHandler.postDelayed(
                { NativeBridge.writeInput(targetId, bytes) },
                HEADLESS_X11_INJECT_DELAY_MS,
            )
        }
        val waiter = object : Runnable {
            override fun run() {
                polls += 1
                if (x0.exists() || polls >= X11_SOCKET_WAIT_MAX_POLLS) {
                    inject()
                    return
                }
                headlessInjectHandler.postDelayed(this, X11_SOCKET_WAIT_POLL_MS)
            }
        }
        headlessInjectHandler.post(waiter)
    }

    fun buildWaylandAndGraphicsEnvSnippet(socketName: String, vulkanMode: String, openGLMode: String): String {
        val b = StringBuilder()
        b.append("export WAYLAND_DISPLAY=").append(socketName).append("\n")

        // OpenGL selection (Desktop is managed by Trierarch; user overrides are not supported here).
        if (openGLMode == "VIRGL") {
            b.append("export GALLIUM_DRIVER=virpipe\n")
            b.append("export MESA_LOADER_DRIVER_OVERRIDE=virpipe\n")
            b.append("export LIBGL_ALWAYS_SOFTWARE=0\n")
            b.append("export VTEST_SOCKET_NAME=/run/trierarch-virgl/vtest.sock\n")
            b.append("export VTEST_RENDERER_SOCKET_NAME=/run/trierarch-virgl/vtest.sock\n")
        } else {
            b.append("export GALLIUM_DRIVER=llvmpipe\n")
            b.append("export MESA_LOADER_DRIVER_OVERRIDE=llvmpipe\n")
            b.append("export LIBGL_ALWAYS_SOFTWARE=1\n")
        }

        // Vulkan selection.
        if (vulkanMode == "VENUS") {
            // Force virtio/venus ICD to avoid freedreno interference.
            b.append("export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/virtio_icd.json\n")
            b.append("export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/virtio_icd.json\n")
            b.append("export VN_DEBUG=vtest\n")
        } else {
            // Avoid inheriting a stale ICD from a previous mode.
            b.append("unset VK_ICD_FILENAMES VK_DRIVER_FILES VN_DEBUG || true\n")
        }
        return b.toString()
    }

    fun runArchWaylandStartupScriptIfNeeded(
        prefs: android.content.SharedPreferences,
        desktopSocketName: String,
        vulkanMode: String,
        openGLMode: String,
        currentHiddenInjectedKey: String,
    ): WaylandEnvState {
        val hasClients = try {
            WaylandBridge.nativeHasActiveClients()
        } catch (_: Throwable) {
            false
        }

        val hiddenKey = "$desktopSocketName|$vulkanMode|$openGLMode"

        ensureArchWaylandDisplaySession()
        if (currentHiddenInjectedKey != hiddenKey) {
            NativeBridge.writeInput(
                TerminalSessionIds.ARCH_WAYLAND_DISPLAY,
                buildWaylandAndGraphicsEnvSnippet(desktopSocketName, vulkanMode, openGLMode)
                    .toByteArray(Charsets.UTF_8)
            )
        }

        if (!hasClients) {
            val script = prefs.getString("desktop_startup_script", "")?.trim()
            if (!script.isNullOrEmpty()) {
                ensureArchWaylandDisplaySession()
                NativeBridge.writeInput(
                    TerminalSessionIds.ARCH_WAYLAND_DISPLAY,
                    (script + "\n").toByteArray(Charsets.UTF_8)
                )
            }
        }
        return WaylandEnvState(hiddenInjectedKey = hiddenKey)
    }
}

