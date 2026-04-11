package app.trierarch

/** JNI: `libtrierarch.so`. Call [init] before other methods. */
object NativeBridge {
    init {
        System.loadLibrary("trierarch")
    }

    external fun init(dataDir: String, cacheDir: String, nativeLibraryDir: String, externalStorageDir: String?): Boolean

    external fun stopVirglHost()

    /** `"LLVMPIPE"` | `"UNIVERSAL"`. */
    external fun setRendererMode(mode: String)

    external fun hasRootfs(): Boolean

    external fun downloadRootfs(callback: ProgressCallback): Boolean

    external fun spawnSession(sessionId: Int, rows: Int, cols: Int): Boolean

    external fun closeSession(sessionId: Int)

    external fun isSessionAlive(sessionId: Int): Boolean

    external fun writeInput(sessionId: Int, bytes: ByteArray)

    external fun setPtyWindowSize(sessionId: Int, rows: Int, cols: Int)
}

interface ProgressCallback {
    fun onProgress(pct: Int, msg: String)
}
