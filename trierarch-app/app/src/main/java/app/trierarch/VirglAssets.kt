package app.trierarch

import android.content.Context
import android.content.res.AssetManager
import android.system.ErrnoException
import android.system.Os
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/**
 * Extracts host-side VirGL + ANGLE binaries from APK assets.
 *
 * Layout:
 * - Immutable trees: `files/virgl.payload.<stamp>/` (bin, lib, angle, `.stamp`)
 * - Stable entry: `files/virgl` → **symlink** to the active payload directory (relative target).
 *
 * Updates **never delete or rename** a payload directory that may still be mmap'd by a lingering
 * `virgl_render_server` / linker child. We only add a new `virgl.payload.*` and retarget the
 * symlink. Old payloads are left on disk (small); optional GC could run on a future boot after stop.
 */
object VirglAssets {
    private const val ASSET_ROOT = "virgl"
    private const val LIVE_LINK_NAME = "virgl"
    private const val STAGING_DIR = "virgl.staging"
    private const val PAYLOAD_PREFIX = "virgl.payload."

    private val SHARED_LIBS = arrayOf("libvirglrenderer.so", "libepoxy.so")

    private val ANGLE_BACKENDS = arrayOf("vulkan", "gl", "vulkan-null")
    private val ANGLE_LIBS = arrayOf("libEGL_angle.so", "libGLESv1_CM_angle.so", "libGLESv2_angle.so",
        "libfeature_support_angle.so")

    fun syncFromAssetsIfNeeded(context: Context) {
        synchronized(this) {
            syncFromAssetsIfNeededLocked(context)
        }
    }

    private fun resolveActivePayload(filesDir: File): File? {
        val link = File(filesDir, LIVE_LINK_NAME)
        if (!link.exists()) return null
        return try {
            val target = Os.readlink(link.absolutePath)
            val resolved = if (target.startsWith("/")) File(target) else File(filesDir, target)
            resolved.takeIf { it.isDirectory }
        } catch (_: ErrnoException) {
            link.takeIf { it.isDirectory }
        }
    }

    private fun migrateLegacyPlainVirglDirToSymlink(filesDir: File): Boolean {
        val plain = File(filesDir, LIVE_LINK_NAME)
        if (!plain.exists() || !plain.isDirectory) return true
        val legacyName = "${PAYLOAD_PREFIX}legacy_${System.currentTimeMillis()}"
        val legacy = File(filesDir, legacyName)
        if (!plain.renameTo(legacy)) return false
        return try {
            Os.symlink(legacyName, plain.absolutePath)
            true
        } catch (_: ErrnoException) {
            legacy.renameTo(plain)
            false
        }
    }

    private fun syncFromAssetsIfNeededLocked(context: Context) {
        val am = context.assets
        if (am.list(ASSET_ROOT).isNullOrEmpty()) return

        val abi = "arm64-v8a"
        val assetBin = "$ASSET_ROOT/$abi/virgl_test_server_android"
        val filesDir = context.filesDir

        val currentStamp = try {
            val pi = context.packageManager.getPackageInfo(context.packageName, 0)
            "${pi.versionCode}_${pi.lastUpdateTime}"
        } catch (_: Throwable) {
            "unknown"
        }
        val safeStamp = currentStamp.replace(Regex("[^a-zA-Z0-9_.-]"), "_")

        val active = resolveActivePayload(filesDir)
        val stampFile = active?.let { File(it, ".stamp") }
        val stampMatch = stampFile?.isFile == true && stampFile.readText().trim() == currentStamp
        val exe = active?.let { File(it, "bin/virgl_test_server_android") }
        if (stampMatch && exe != null && exe.isFile && exe.canExecute()) return

        if (!migrateLegacyPlainVirglDirToSymlink(filesDir)) return

        val stagingRoot = File(filesDir, STAGING_DIR)
        // Unique directory per extract so we never delete/rename a tree that may still be mmap'd.
        val newPayload = File(filesDir, "${PAYLOAD_PREFIX}${safeStamp}_${System.nanoTime()}")
        val linkFile = File(filesDir, LIVE_LINK_NAME)

        try {
            stagingRoot.deleteRecursively()

            val destBinDir = File(stagingRoot, "bin").apply { mkdirs() }
            val destLibDir = File(stagingRoot, "lib").apply { mkdirs() }
            val destStaging = File(destBinDir, "virgl_test_server_android")

            copyAssetFile(am, assetBin, destStaging)
            destStaging.setExecutable(true, false)

            val renderSrvAsset = "$ASSET_ROOT/$abi/virgl_render_server"
            try {
                val renderSrv = File(destBinDir, "virgl_render_server")
                copyAssetFile(am, renderSrvAsset, renderSrv)
                renderSrv.setExecutable(true, false)
            } catch (_: IOException) {}

            for (lib in SHARED_LIBS) {
                val assetLib = "$ASSET_ROOT/$abi/$lib"
                try {
                    copyAssetFile(am, assetLib, File(destLibDir, lib))
                } catch (_: IOException) {}
                try {
                    copyAssetFile(am, assetLib, File(destBinDir, lib))
                } catch (_: IOException) {}
            }

            syncAngleLibs(am, abi, stagingRoot)

            if (!stagingRoot.renameTo(newPayload)) {
                newPayload.deleteRecursively()
                return
            }
            File(newPayload, ".stamp").writeText(currentStamp)

            if (linkFile.exists()) {
                linkFile.delete()
            }
            try {
                Os.symlink(newPayload.name, linkFile.absolutePath)
            } catch (_: ErrnoException) {
                if (!newPayload.renameTo(linkFile)) {
                    return
                }
            }
        } catch (_: Throwable) {
            if (stagingRoot.exists()) stagingRoot.deleteRecursively()
            if (newPayload.exists()) {
                val pointed = resolveActivePayload(filesDir)?.absolutePath == newPayload.absolutePath
                if (!pointed) newPayload.deleteRecursively()
            }
        }
    }

    private fun syncAngleLibs(am: AssetManager, abi: String, virglRoot: File) {
        for (backend in ANGLE_BACKENDS) {
            val assetDir = "$ASSET_ROOT/$abi/angle/$backend"
            val destDir = File(virglRoot, "angle/$backend").apply { mkdirs() }
            for (lib in ANGLE_LIBS) {
                val assetPath = "$assetDir/$lib"
                val destFile = File(destDir, lib)
                if (!destFile.isFile) {
                    try {
                        copyAssetFile(am, assetPath, destFile)
                    } catch (_: IOException) {}
                }
            }
        }
    }

    private fun copyAssetFile(am: AssetManager, path: String, out: File) {
        out.parentFile?.mkdirs()
        am.open(path).use { input ->
            FileOutputStream(out).use { input.copyTo(it) }
        }
    }
}
