package app.trierarch.ui.runtime

import android.content.SharedPreferences
import app.trierarch.NativeBridge

/**
 * Desktop graphics mode selection persisted in prefs and applied to native runtime.
 *
 * Contract:
 * - Values are sanitized against the provided allowed lists.
 * - When either Vulkan=VENUS or OpenGL=VIRGL is selected, the virgl host is started if possible.
 * - Callers should reset/recreate PTY sessions after a mode change (env is fixed at spawn time).
 */
object GraphicsModeController {

    private const val KEY_VULKAN = "desktop_vulkan_mode"
    private const val KEY_OPENGL = "desktop_opengl_mode"

    data class Modes(
        val vulkan: String,
        val openGL: String,
    )

    fun loadFromPrefs(
        prefs: SharedPreferences,
        allowedVulkan: List<String>,
        allowedOpenGL: List<String>,
        defaultVulkan: String = "LLVMPIPE",
        defaultOpenGL: String = "LLVMPIPE",
    ): Modes {
        val vkRaw = prefs.getString(KEY_VULKAN, defaultVulkan) ?: defaultVulkan
        val glRaw = prefs.getString(KEY_OPENGL, defaultOpenGL) ?: defaultOpenGL
        return sanitize(Modes(vkRaw, glRaw), allowedVulkan, allowedOpenGL, defaultVulkan, defaultOpenGL)
    }

    fun persist(
        prefs: SharedPreferences,
        modes: Modes,
    ) {
        prefs.edit()
            .putString(KEY_VULKAN, modes.vulkan)
            .putString(KEY_OPENGL, modes.openGL)
            .apply()
    }

    fun sanitize(
        modes: Modes,
        allowedVulkan: List<String>,
        allowedOpenGL: List<String>,
        defaultVulkan: String = "LLVMPIPE",
        defaultOpenGL: String = "LLVMPIPE",
    ): Modes {
        val vk = if (modes.vulkan in allowedVulkan) modes.vulkan else defaultVulkan
        val gl = if (modes.openGL in allowedOpenGL) modes.openGL else defaultOpenGL
        return Modes(vulkan = vk, openGL = gl)
    }

    /**
     * Persists [modes] and updates virgl host state as needed.
     *
     * @return true if the mode actually changed compared to [previous].
     */
    fun applyAndMaybeToggleVirglHost(
        prefs: SharedPreferences,
        previous: Modes,
        modes: Modes,
    ): Boolean {
        val changed = previous != modes
        persist(prefs, modes)

        try {
            if (modes.vulkan == "VENUS" || modes.openGL == "VIRGL") {
                NativeBridge.startVirglHostIfPossible()
            } else {
                NativeBridge.stopVirglHost()
            }
        } catch (_: Throwable) {
        }
        return changed
    }
}

