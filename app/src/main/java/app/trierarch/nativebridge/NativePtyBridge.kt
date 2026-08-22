package app.trierarch.nativebridge

/** The intentionally small Kotlin boundary around `libtrierarch_native.so`. */
object NativePtyBridge {
    init {
        System.loadLibrary("trierarch_native")
    }

    external fun openSession(
        command: String,
        arguments: Array<String>,
        workingDirectory: String,
        environment: Array<String>,
        rows: Int,
        columns: Int,
        callback: SessionCallback,
    ): Long

    /** Opens PRoot through the native runtime backend, not the generic command path. */
    external fun openProotSession(
        rootfsDirectory: String,
        shell: String,
        nativeLibraryDirectory: String,
        cacheDirectory: String,
        x11SocketDirectory: String,
        launchArgv: Array<String>,
        rows: Int,
        columns: Int,
        callback: SessionCallback,
    ): Long

    /** Opens an already deployed rootfs through the device's existing `su` provider. */
    external fun openChrootSession(
        rootfsDirectory: String,
        shell: String,
        x11SocketDirectory: String,
        launchArgv: Array<String>,
        rows: Int,
        columns: Int,
        callback: SessionCallback,
    ): Long

    /** Starts a stopped DroidSpaces container, or attaches to a running one. */
    external fun openDroidspacesSession(
        container: String,
        user: String,
        x11SocketDirectory: String,
        waylandRuntimeDirectory: String,
        launchArgv: Array<String>,
        rows: Int,
        columns: Int,
        callback: SessionCallback,
    ): Long

    /** Restores ownership of the private X11 socket directory after a legacy directory bind. */
    external fun repairX11SocketDirectory(directory: String, appUid: Int)

    external fun isDroidspacesRunning(container: String): Boolean

    external fun stopDroidspaces(container: String)

    external fun write(sessionId: Long, bytes: ByteArray): Boolean

    external fun resize(sessionId: Long, rows: Int, columns: Int): Boolean

    external fun close(sessionId: Long)
}

interface SessionCallback {
    fun onSessionOutput(sessionId: Long, bytes: ByteArray)

    /** A conventional process exit value: signal exits are represented as 128 + signal. */
    fun onSessionExited(sessionId: Long, exitCode: Int)
}
