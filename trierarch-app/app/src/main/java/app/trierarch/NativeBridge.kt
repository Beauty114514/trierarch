package app.trierarch

/**
 * JNI bridge to trierarch. Load the library before calling any method.
 */
object NativeBridge {
    init {
        System.loadLibrary("trierarch")
    }

    /** Initialize with app paths. Call first. externalStorageDir: optional (e.g. Environment.getExternalStorageDirectory()?.absolutePath); when set, proot binds it as /android and /root/Android like LocalDesktop. */
    external fun init(dataDir: String, cacheDir: String, nativeLibraryDir: String, externalStorageDir: String?): Boolean

    /** True if Arch rootfs exists. */
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
