package app.trierarch.ui.runtime

import android.content.Context
import android.content.SharedPreferences
import android.os.Environment
import app.trierarch.NativeBridge
import app.trierarch.ProgressCallback
import app.trierarch.PulseAssets
import app.trierarch.VirglAssets
import app.trierarch.ui.prefs.AppPrefs

/**
 * Initializes the JNI/native layer, syncs bundled assets, and downloads rootfs payloads.
 *
 * This is intentionally UI-agnostic: the caller decides how to represent progress and errors.
 */
object NativeInstallCoordinator {

    data class InitResult(
        val ok: Boolean,
        val hasArchRootfs: Boolean,
        val hasDebianRootfs: Boolean,
        val hasWineRootfs: Boolean,
        val desktopModes: GraphicsModeController.Modes,
    )

    suspend fun initNativeAndSyncAssets(
        context: Context,
        prefs: SharedPreferences,
        allowedVulkan: List<String>,
        allowedOpenGL: List<String>,
    ): InitResult {
        // Renderer migration must happen before we read the new keys.
        migrateRendererPrefsIfNeeded(prefs)

        val desktopModes = GraphicsModeController.loadFromPrefs(
            prefs = prefs,
            allowedVulkan = allowedVulkan,
            allowedOpenGL = allowedOpenGL,
        )

        val ok = kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.IO) {
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

        // Remove legacy preference key (no longer supported).
        prefs.edit().remove("display_startup_script").apply()

        if (!ok) {
            return InitResult(
                ok = false,
                hasArchRootfs = false,
                hasDebianRootfs = false,
                hasWineRootfs = false,
                desktopModes = desktopModes,
            )
        }

        return InitResult(
            ok = true,
            hasArchRootfs = NativeBridge.hasArchRootfs(),
            hasDebianRootfs = NativeBridge.hasDebianRootfs(),
            hasWineRootfs = NativeBridge.hasWineRootfs(),
            desktopModes = desktopModes,
        )
    }

    data class DownloadResult(
        val archOk: Boolean,
        val debianOk: Boolean,
        val wineOk: Boolean,
        val hasArchRootfs: Boolean,
        val hasDebianRootfs: Boolean,
        val hasWineRootfs: Boolean,
    ) {
        val allOk: Boolean get() = archOk && debianOk && wineOk
    }

    suspend fun downloadMissingRootfsSequentially(
        onProgress: (pct: Int, msg: String) -> Unit,
    ): DownloadResult {
        return kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.IO) {
            val archOk = if (!NativeBridge.hasArchRootfs()) {
                NativeBridge.downloadArchRootfs(object : ProgressCallback {
                    override fun onProgress(pct: Int, msg: String) = onProgress(pct, msg)
                })
            } else true

            val debOk = if (archOk && !NativeBridge.hasDebianRootfs()) {
                NativeBridge.downloadDebianRootfs(object : ProgressCallback {
                    override fun onProgress(pct: Int, msg: String) = onProgress(pct, msg)
                })
            } else archOk

            val wineOk = if (debOk && !NativeBridge.hasWineRootfs()) {
                NativeBridge.downloadWineRootfs(object : ProgressCallback {
                    override fun onProgress(pct: Int, msg: String) = onProgress(pct, msg)
                })
            } else debOk

            DownloadResult(
                archOk = archOk,
                debianOk = debOk,
                wineOk = wineOk,
                hasArchRootfs = NativeBridge.hasArchRootfs(),
                hasDebianRootfs = NativeBridge.hasDebianRootfs(),
                hasWineRootfs = NativeBridge.hasWineRootfs(),
            )
        }
    }

    private fun migrateRendererPrefsIfNeeded(prefs: SharedPreferences) {
        val rawLegacy = prefs.getString("desktop_renderer_mode", "") ?: ""
        val (migrated, shouldRemoveLegacy) = AppPrefs.migrateLegacyRendererMode(rawLegacy)
        if (migrated != null) {
            prefs.edit()
                .remove("desktop_renderer_mode")
                .putString("desktop_vulkan_mode", migrated.first)
                .putString("desktop_opengl_mode", migrated.second)
                .apply()
        } else if (shouldRemoveLegacy) {
            prefs.edit().remove("desktop_renderer_mode").apply()
        }
    }
}

