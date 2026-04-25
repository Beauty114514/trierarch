package app.trierarch

import android.app.Application
import android.content.Context
import android.content.Intent
import android.os.Build
import android.system.Os
import android.util.Log
import app.trierarch.wayland.input.InputRouteState
import com.termux.x11.X11ServerService
import java.io.BufferedInputStream
import java.io.File
import java.io.FileOutputStream
import java.util.zip.ZipInputStream

/**
 * X11 display server: **Termux / upstream model** — [com.termux.x11.CmdEntryPoint] runs in a separate
 * process ([X11ServerService] `:x11`, same as invoking `app_process` + `main`). The Lorie
 * [com.termux.x11.LorieView] and [com.termux.x11.MainActivity] stay in the main app process
 * (two "windows": shell + full-screen Lorie) without running Xorg in that process.
 *
 * Only the main process should call [initInMainProcess] (LorieView JNI) and
 * [ensureX11ServerProcessStarted]. The `:x11` process has its own [Application] instance; env for
 * CmdEntryPoint is applied in [X11ServerService] before [com.termux.x11.CmdEntryPoint.main].
 */
object X11Runtime {
    private const val TAG = "TrierarchX11"

    private const val BUNDLED_XKB_ASSET = "lorie_xkb_bundled.zip"
    private const val BUNDLED_XKB_UNPACKED_SENTINEL = "usr/share/X11/xkb/.trierarch_bundled_xkb_ok"

    @JvmStatic
    fun isMainAppProcess(app: Application): Boolean {
        return if (Build.VERSION.SDK_INT >= 28) {
            app.packageName == Application.getProcessName()
        } else {
            val am = app.getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
            val p = am.runningAppProcesses?.find { it.pid == android.os.Process.myPid() }
            p?.processName == app.packageName
        }
    }

    /**
     * Main process: load Lorie for [LorieView]. Secondary processes (e.g. `:x11`) must not
     * double‑register with this object — CmdEntryPoint loads the library there.
     */
    @JvmStatic
    fun initInMainProcess(app: Application) {
        if (!isMainAppProcess(app)) {
            return
        }
        try {
            System.loadLibrary("Xlorie")
            Log.i(TAG, "libXlorie (main, Lorie client) loaded")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "libXlorie missing from APK (main)", e)
        }
    }

    /**
     * Unpacks xkb, sets [TMPDIR] / [XKB_CONFIG_ROOT] for the **current** process, then the caller
     * may run [com.termux.x11.CmdEntryPoint]. Must not run on the app UI main thread.
     */
    @JvmStatic
    @Synchronized
    fun applyLorieProcessEnvForX11ServerProcess(app: Application) {
        val root = app.filesDir
        unpackBundledLorieXkbIfNeeded(app, root)
        val tmp = File(root, "tmp")
        try {
            tmp.mkdirs()
            Os.setenv("TMPDIR", tmp.absolutePath, true)
        } catch (e: Exception) {
            Log.w(TAG, "setenv TMPDIR", e)
        }
        val xkbX11 = File(root, "usr/share/X11/xkb")
        val xkbAlt = File(root, "usr/share/xkeyboard-config-2")
        try {
            when {
                xkbX11.isDirectory -> Os.setenv("XKB_CONFIG_ROOT", xkbX11.absolutePath, true)
                xkbAlt.isDirectory -> Os.setenv("XKB_CONFIG_ROOT", xkbAlt.absolutePath, true)
            }
        } catch (e: Exception) {
            Log.w(TAG, "setenv XKB_CONFIG_ROOT", e)
        }
        Log.i(
            TAG,
            "Lorie (X11 server) env: TMPDIR=${System.getenv("TMPDIR")} XKB_CONFIG_ROOT=${System.getenv("XKB_CONFIG_ROOT")} " +
                "xkbX11=${xkbX11.isDirectory} xkbAlt=${xkbAlt.isDirectory}"
        )
    }

    @JvmStatic
    fun ensureX11ServerProcessStarted(context: Context) {
        val appCtx = context.applicationContext
        val it = Intent(appCtx, X11ServerService::class.java)
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                appCtx.startForegroundService(it)
            } else {
                @Suppress("DEPRECATION")
                appCtx.startService(it)
            }
        } catch (e: Exception) {
            Log.e(TAG, "start X11ServerService", e)
        }
    }

    /**
     * Called from [com.termux.x11.MainActivity] (same process as Lorie) for input routing.
     */
    fun setLorieInputRouteTopVisible(visible: Boolean) {
        InputRouteState.lorieX11DisplayVisible = visible
    }

    private fun unpackBundledLorieXkbIfNeeded(app: Application, filesRoot: File) {
        val sentinel = File(filesRoot, BUNDLED_XKB_UNPACKED_SENTINEL)
        if (sentinel.isFile) {
            return
        }
        val rules = File(filesRoot, "usr/share/X11/xkb/rules/evdev")
        val rulesXml = File(filesRoot, "usr/share/X11/xkb/rules/evdev.xml")
        if (rules.isFile || rulesXml.isFile) {
            runCatching { sentinel.createNewFile() }
            return
        }
        if (app.assets.list("")?.contains(BUNDLED_XKB_ASSET) != true) {
            Log.w(TAG, "No $BUNDLED_XKB_ASSET; Lorie :x11 may need rootfs or embedded xkb")
            return
        }
        try {
            val rootCanon = filesRoot.canonicalFile
            app.assets.open(BUNDLED_XKB_ASSET).use { a ->
                BufferedInputStream(a).use { bi ->
                    ZipInputStream(bi).use { zis ->
                        var e = zis.nextEntry
                        while (e != null) {
                            if (!e.isDirectory) {
                                var name = e.name
                                if (name.contains("..")) {
                                    e = zis.nextEntry
                                    continue
                                }
                                if (name.startsWith("/")) {
                                    name = name.removePrefix("/")
                                }
                                val out = File(filesRoot, name)
                                val c = out.canonicalPath
                                if (!c.startsWith(rootCanon.path + File.separator) && c != rootCanon.path) {
                                    e = zis.nextEntry
                                    continue
                                }
                                out.parentFile?.mkdirs()
                                FileOutputStream(out).use { o -> zis.copyTo(o) }
                            }
                            e = zis.nextEntry
                        }
                    }
                }
            }
            runCatching { File(filesRoot, BUNDLED_XKB_UNPACKED_SENTINEL).createNewFile() }
            Log.i(TAG, "xkb: unpacked $BUNDLED_XKB_ASSET (one-time, :x11 or main thread)")
        } catch (e: Exception) {
            Log.e(TAG, "Unpacking $BUNDLED_XKB_ASSET failed", e)
        }
    }
}
