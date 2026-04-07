package app.trierarch

import android.content.Context
import android.content.res.AssetManager
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/**
 * Copies a bundled Pulse prefix from [assets]/pulse into [Context.getFilesDir]/pulse so native
 * code can execute [pulse]/bin/pulseaudio (assets are not executable). Skips work when no pulse
 * assets are packaged or when the on-disk tree already matches [.pack_stamp] from the build.
 */
object PulseAssets {
    private const val ASSET_ROOT = "pulse"

    /** Sysroot / runtime deps shipped under [lib/]; must exist or pulseaudio fails at link time. */
    private val REQUIRED_LIB_FILES = arrayOf(
        "lib/libltdl.so",
        "lib/libsndfile.so",
    )

    private fun pulseExtractLooksComplete(dest: File): Boolean {
        val bin = File(dest, "bin/pulseaudio")
        if (!bin.isFile) return false
        return REQUIRED_LIB_FILES.all { dest.resolve(it).isFile }
    }

    fun syncFromAssetsIfNeeded(context: Context) {
        val am = context.assets
        if (am.list(ASSET_ROOT).isNullOrEmpty()) return

        val dest = File(context.filesDir, "pulse")
        val stampAsset = readAssetTextOrNull(am, "$ASSET_ROOT/.pack_stamp")?.trim().orEmpty()
        val stampLocal = dest.resolve(".pack_stamp").takeIf { it.isFile }?.readText()?.trim().orEmpty()
        val binary = File(dest, "bin/pulseaudio")
        if (binary.isFile && pulseExtractLooksComplete(dest)) {
            // Stamp mismatch → resync. No stamp in APK but tree complete → skip (avoids churn).
            if (stampAsset.isNotEmpty() && stampAsset == stampLocal) return
            if (stampAsset.isEmpty()) return
        }

        dest.deleteRecursively()
        dest.mkdirs()
        copyAssetTree(am, ASSET_ROOT, dest)
        binary.takeIf { it.isFile }?.setExecutable(true, false)
        if (stampAsset.isNotEmpty()) {
            dest.resolve(".pack_stamp").writeText(stampAsset)
        }
    }

    private fun readAssetTextOrNull(am: AssetManager, path: String): String? = try {
        am.open(path).bufferedReader().use { it.readText() }
    } catch (_: IOException) {
        null
    }

    private fun copyAssetTree(am: AssetManager, assetPath: String, outDir: File) {
        val names = am.list(assetPath) ?: return
        if (names.isEmpty()) return
        for (name in names) {
            if (name.isEmpty()) continue
            val sub = "$assetPath/$name"
            val out = File(outDir, name)
            val nested = am.list(sub)
            if (nested != null && nested.isNotEmpty()) {
                out.mkdirs()
                copyAssetTree(am, sub, out)
            } else {
                out.parentFile?.mkdirs()
                try {
                    am.open(sub).use { input ->
                        FileOutputStream(out).use { input.copyTo(it) }
                    }
                } catch (_: IOException) {
                    out.mkdirs()
                }
            }
        }
    }
}
