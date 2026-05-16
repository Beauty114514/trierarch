package app.trierarch.ui.runtime

import android.content.Context
import android.content.SharedPreferences
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

    data class WaylandEnvInjectResult(
        val envInjectKey: String,
    )

    fun prepareWaylandCompositor(context: Context, waylandRuntimeDir: String): Boolean {
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

    fun ensureArchWaylandHeadlessSession() {
        if (!NativeBridge.isSessionAlive(TerminalSessionIds.ARCH_WAYLAND_HEADLESS)) {
            NativeBridge.spawnSession(TerminalSessionIds.ARCH_WAYLAND_HEADLESS, 24, 80)
        }
    }

    fun ensureArchX11HeadlessSession(): Boolean {
        if (NativeBridge.isSessionAlive(TerminalSessionIds.ARCH_X11_HEADLESS)) return true
        return NativeBridge.spawnSession(TerminalSessionIds.ARCH_X11_HEADLESS, 24, 80)
    }

    fun ensureDebianWaylandHeadlessSession(hasDebianRootfs: Boolean): Boolean {
        if (!hasDebianRootfs) return false
        if (NativeBridge.isSessionAlive(TerminalSessionIds.DEBIAN_WAYLAND_HEADLESS)) return true
        return NativeBridge.spawnSessionInRootfs(
            TerminalSessionIds.DEBIAN_WAYLAND_HEADLESS,
            24,
            80,
            TerminalSessionIds.rootfsKindForNativeId(TerminalSessionIds.DEBIAN_WAYLAND_HEADLESS),
        )
    }

    fun ensureDebianX11HeadlessSession(hasDebianRootfs: Boolean): Boolean {
        if (!hasDebianRootfs) return false
        if (NativeBridge.isSessionAlive(TerminalSessionIds.DEBIAN_X11_HEADLESS)) return true
        return NativeBridge.spawnSessionInRootfs(
            TerminalSessionIds.DEBIAN_X11_HEADLESS,
            24,
            80,
            TerminalSessionIds.rootfsKindForNativeId(TerminalSessionIds.DEBIAN_X11_HEADLESS),
        )
    }

    fun injectDebianX11Startup(
        context: Context,
        prefs: SharedPreferences,
        headlessInjectHandler: Handler,
        hasDebianRootfs: Boolean,
    ) {
        if (!ensureDebianX11HeadlessSession(hasDebianRootfs)) return
        injectX11Startup(
            context = context,
            headlessInjectHandler = headlessInjectHandler,
            headlessSessionId = TerminalSessionIds.DEBIAN_X11_HEADLESS,
            userScript = AppPrefs.readDebianX11StartupScript(prefs),
        )
    }

    fun injectArchX11Startup(
        context: Context,
        prefs: SharedPreferences,
        headlessInjectHandler: Handler,
    ) {
        if (!ensureArchX11HeadlessSession()) return
        injectX11Startup(
            context = context,
            headlessInjectHandler = headlessInjectHandler,
            headlessSessionId = TerminalSessionIds.ARCH_X11_HEADLESS,
            userScript = AppPrefs.readArchX11StartupScript(prefs),
        )
    }

    private fun injectX11Startup(
        context: Context,
        headlessInjectHandler: Handler,
        headlessSessionId: Int,
        userScript: String,
    ) {
        val user = userScript.trim()
        val payload = buildString {
            append(AppPrefs.buildX11ShellEnvSnippet())
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
                { NativeBridge.writeInput(headlessSessionId, bytes) },
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

    fun buildWaylandGraphicsEnvSnippet(socketName: String, vulkanMode: String, openGLMode: String): String {
        val b = StringBuilder()
        b.append("export WAYLAND_DISPLAY=").append(socketName).append("\n")

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

        if (vulkanMode == "VENUS") {
            b.append("export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/virtio_icd.json\n")
            b.append("export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/virtio_icd.json\n")
            b.append("export VN_DEBUG=vtest\n")
        } else {
            b.append("unset VK_ICD_FILENAMES VK_DRIVER_FILES VN_DEBUG || true\n")
        }
        return b.toString()
    }

    fun injectArchWaylandStartupIfNeeded(
        prefs: SharedPreferences,
        waylandSocketName: String,
        vulkanMode: String,
        openGLMode: String,
        currentEnvInjectKey: String,
    ): WaylandEnvInjectResult {
        val hasClients = try {
            WaylandBridge.nativeHasActiveClients()
        } catch (_: Throwable) {
            false
        }

        val envInjectKey = "arch|$waylandSocketName|$vulkanMode|$openGLMode"

        ensureArchWaylandHeadlessSession()
        if (currentEnvInjectKey != envInjectKey) {
            NativeBridge.writeInput(
                TerminalSessionIds.ARCH_WAYLAND_HEADLESS,
                buildWaylandGraphicsEnvSnippet(waylandSocketName, vulkanMode, openGLMode)
                    .toByteArray(Charsets.UTF_8),
            )
        }

        if (!hasClients) {
            val script = AppPrefs.readArchWaylandStartupScript(prefs)
            if (script.isNotEmpty()) {
                ensureArchWaylandHeadlessSession()
                NativeBridge.writeInput(
                    TerminalSessionIds.ARCH_WAYLAND_HEADLESS,
                    (script + "\n").toByteArray(Charsets.UTF_8),
                )
            }
        }
        return WaylandEnvInjectResult(envInjectKey = envInjectKey)
    }

    fun injectDebianWaylandStartupIfNeeded(
        prefs: SharedPreferences,
        waylandSocketName: String,
        vulkanMode: String,
        openGLMode: String,
        currentEnvInjectKey: String,
        hasDebianRootfs: Boolean,
    ): WaylandEnvInjectResult {
        val hasClients = try {
            WaylandBridge.nativeHasActiveClients()
        } catch (_: Throwable) {
            false
        }

        val envInjectKey = "debian|$waylandSocketName|$vulkanMode|$openGLMode"

        if (!ensureDebianWaylandHeadlessSession(hasDebianRootfs)) {
            return WaylandEnvInjectResult(envInjectKey = currentEnvInjectKey)
        }
        if (currentEnvInjectKey != envInjectKey) {
            NativeBridge.writeInput(
                TerminalSessionIds.DEBIAN_WAYLAND_HEADLESS,
                buildWaylandGraphicsEnvSnippet(waylandSocketName, vulkanMode, openGLMode)
                    .toByteArray(Charsets.UTF_8),
            )
        }

        if (!hasClients) {
            val script = AppPrefs.readDebianWaylandStartupScript(prefs)
            if (script.isNotEmpty()) {
                ensureDebianWaylandHeadlessSession(hasDebianRootfs)
                NativeBridge.writeInput(
                    TerminalSessionIds.DEBIAN_WAYLAND_HEADLESS,
                    (script + "\n").toByteArray(Charsets.UTF_8),
                )
            }
        }
        return WaylandEnvInjectResult(envInjectKey = envInjectKey)
    }
}
