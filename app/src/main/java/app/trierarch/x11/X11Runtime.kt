package app.trierarch.x11

import android.content.Context
import android.os.Process
import android.system.Os
import app.trierarch.nativebridge.NativePtyBridge
import java.io.BufferedInputStream
import java.io.File
import java.io.FileOutputStream
import java.util.zip.ZipInputStream

/** Server-process setup for the runtime files shipped by trierarch-x11-host. */
object X11Runtime {
    private const val XKB_ASSET = "lorie_xkb_bundled.zip"
    private const val SENTINEL = ".trierarch-xkb-ready"
    private const val X11_SOCKET_DIRECTORY_MODE = 0b1111111111 // 01777

    @JvmStatic
    fun prepareServerEnvironment(context: Context) {
        val root = File(context.filesDir, "x11")
        val xkbRoot = File(root, "usr/share/X11/xkb")
        if (!File(xkbRoot, SENTINEL).isFile) unpackXkb(context, root, xkbRoot)
        val tmp = File(root, "tmp").also { check(it.isDirectory || it.mkdirs()) { "Unable to create X11 tmp" } }
        prepareSocketDirectory(context)
        check(xkbRoot.isDirectory) { "Lorie XKB rules were not installed" }
        Os.setenv("TMPDIR", tmp.absolutePath, true)
        Os.setenv("XKB_CONFIG_ROOT", xkbRoot.absolutePath, true)
    }

    /** The host-side directory containing Lorie's X0 socket. */
    @JvmStatic
    fun socketDirectory(context: Context): File = File(context.filesDir, "x11/tmp/.X11-unix")

    /**
     * The old directory bind could leave this private directory root-owned.
     * Recover it once through the root bridge, then keep it exclusively under
     * the app's control; the container now receives only X0, never this dir.
     */
    @JvmStatic
    fun prepareSocketDirectory(context: Context): File = socketDirectory(context).also { directory ->
        check(directory.isDirectory || directory.mkdirs()) { "Unable to create X11 socket directory" }
        runCatching { Os.chmod(directory.absolutePath, X11_SOCKET_DIRECTORY_MODE) }
            .getOrElse {
                NativePtyBridge.repairX11SocketDirectory(directory.absolutePath, Process.myUid())
                Os.chmod(directory.absolutePath, X11_SOCKET_DIRECTORY_MODE)
            }
    }

    private fun unpackXkb(context: Context, root: File, xkbRoot: File) {
        val rootCanonical = root.canonicalFile
        context.assets.open(XKB_ASSET).use { source ->
            ZipInputStream(BufferedInputStream(source)).use { archive ->
                var entry = archive.nextEntry
                while (entry != null) {
                    if (!entry.isDirectory) {
                        val output = File(root, entry.name).canonicalFile
                        require(output.path.startsWith(rootCanonical.path + File.separator)) {
                            "Invalid XKB asset entry: ${entry.name}"
                        }
                        output.parentFile?.mkdirs()
                        FileOutputStream(output).use(archive::copyTo)
                    }
                    entry = archive.nextEntry
                }
            }
        }
        check(File(xkbRoot, "rules/evdev").isFile) { "Bundled XKB rules are incomplete" }
        check(File(xkbRoot, SENTINEL).createNewFile() || File(xkbRoot, SENTINEL).isFile) {
            "Unable to record XKB installation"
        }
    }
}
