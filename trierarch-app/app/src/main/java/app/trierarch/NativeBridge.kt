package app.trierarch

/**
 * Kotlin/JNI bridge to the native runtime (`libtrierarch.so`).
 *
 * Contract:
 * - The native library is loaded once when this object is initialized.
 * - Call [init] exactly once early in app startup before calling any other method.
 * - All methods are thin JNI wrappers; error handling/logging happens in native code.
 */
object NativeBridge {
    init {
        System.loadLibrary("trierarch")
    }

    /**
     * Initialize native layer with app paths. Must be called before any other method.
     *
     * @param dataDir app internal files directory (e.g. `context.filesDir`)
     * @param cacheDir app cache directory (e.g. `context.cacheDir`)
     * @param nativeLibraryDir directory containing packaged `.so` files
     * @param externalStorageDir optional external storage root (e.g. `/storage/emulated/0`).
     * When set, the proot environment will bind it into the guest as `/android` and `/root/Android`.
     */
    external fun init(dataDir: String, cacheDir: String, nativeLibraryDir: String, externalStorageDir: String?): Boolean

    /** True if the distro rootfs is present on disk. */
    external fun hasRootfs(): Boolean

    /** Download rootfs. Blocks until complete. Callback.onProgress(pct, msg). */
    external fun downloadRootfs(callback: ProgressCallback): Boolean

    /** Spawn proot shell under a PTY with initial terminal size (idempotent). Same row/column count as Termux TerminalView. */
    external fun spawnProot(rows: Int, cols: Int): Boolean

    /** True after the first successful [spawnProot]. */
    external fun isProotSpawned(): Boolean

    /** Write bytes to PTY stdin. */
    external fun writeInput(bytes: ByteArray)

    /** Propagate terminal size to the kernel PTY (TIOCSWINSIZE). */
    external fun setPtyWindowSize(rows: Int, cols: Int)
}

interface ProgressCallback {
    fun onProgress(pct: Int, msg: String)
}
