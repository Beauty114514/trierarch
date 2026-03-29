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

    /** Spawn proot shell. */
    external fun spawnProot(): Boolean

    /** Get terminal output lines. */
    external fun getLines(): List<String>

    /** Current line buffer (prompt + echoed input) without trailing newline. */
    external fun getPartialLine(): String

    /** Write bytes to PTY stdin. */
    external fun writeInput(bytes: ByteArray)
}

interface ProgressCallback {
    fun onProgress(pct: Int, msg: String)
}
