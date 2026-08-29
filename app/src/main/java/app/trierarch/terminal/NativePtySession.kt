package app.trierarch.terminal

import android.util.Log
import app.trierarch.config.ProfileStore
import app.trierarch.nativebridge.NativePtyBridge
import app.trierarch.nativebridge.SessionCallback
import app.trierarch.runtime.TerminalLaunchSpec
import com.termux.terminal.TerminalEmulator
import com.termux.terminal.TerminalOutput
import com.termux.terminal.TerminalSession
import com.termux.terminal.TerminalSessionClient
import com.termux.view.DisplayableTermSession
import java.io.File

/**
 * Connects a [TerminalEmulator] to Trierarch's own PTY. It never creates a
 * Termux [TerminalSession], so the native library is the only process owner.
 */
class NativePtySession private constructor(
    private val openNativeSession: (rows: Int, columns: Int, callback: SessionCallback) -> Long,
    private val clipboard: TerminalClipboard,
) : TerminalOutput(), SessionCallback, DisplayableTermSession {
    constructor(
        launchSpec: TerminalLaunchSpec,
        clipboard: TerminalClipboard,
    ) : this(
        openNativeSession = { rows, columns, callback ->
            NativePtyBridge.openSession(
                launchSpec.command,
                launchSpec.arguments,
                launchSpec.workingDirectory.absolutePath,
                launchSpec.environment,
                rows,
                columns,
                callback,
            )
        },
        clipboard = clipboard,
    )

    /** The PRoot runtime has its own native launch contract. */
    constructor(
        rootfsDirectory: File,
        shell: String,
        nativeLibraryDirectory: File,
        cacheDirectory: File,
        x11SocketDirectory: String?,
        launchArgv: Array<String>,
        graphicsEnvironment: Array<String>,
        clipboard: TerminalClipboard,
    ) : this(
        openNativeSession = { rows, columns, callback ->
            NativePtyBridge.openProotSession(
                rootfsDirectory.absolutePath,
                shell,
                nativeLibraryDirectory.absolutePath,
                cacheDirectory.absolutePath,
                x11SocketDirectory.orEmpty(),
                launchArgv,
                graphicsEnvironment,
                rows,
                columns,
                callback,
            )
        },
        clipboard = clipboard,
    )

    /** A shell in a rootfs the user has already deployed and mounted as needed. */
    constructor(
        chrootRootfs: String,
        shell: String,
        x11SocketDirectory: String?,
        launchArgv: Array<String>,
        graphicsEnvironment: Array<String>,
        clipboard: TerminalClipboard,
    ) : this(
        openNativeSession = { rows, columns, callback ->
            NativePtyBridge.openChrootSession(
                chrootRootfs,
                shell,
                x11SocketDirectory.orEmpty(),
                launchArgv,
                graphicsEnvironment,
                rows,
                columns,
                callback,
            )
        },
        clipboard = clipboard,
    )

    /** Trierarch owns the DroidSpaces session lifecycle. */
    constructor(
        droidspacesProfile: ProfileStore.DroidspacesProfile,
        x11SocketDirectory: String?,
        waylandRuntimeDirectory: String?,
        virglRuntimeDirectory: String?,
        clipboard: TerminalClipboard,
    ) : this(
        openNativeSession = { rows, columns, callback ->
            NativePtyBridge.openDroidspacesSession(
                droidspacesProfile.container,
                droidspacesProfile.user,
                x11SocketDirectory.orEmpty(),
                waylandRuntimeDirectory.orEmpty(),
                virglRuntimeDirectory.orEmpty(),
                droidspacesProfile.launchArgv.orEmpty().toTypedArray(),
                droidspacesProfile.graphics.environment().toTypedArray(),
                rows,
                columns,
                callback,
            )
        },
        clipboard = clipboard,
    )

    private var sessionId = 0L
    private var closed = false
    private var onScreenChanged: (() -> Unit)? = null

    private val emulator = TerminalEmulator(
        this,
        INITIAL_COLUMNS,
        INITIAL_ROWS,
        INITIAL_CELL_WIDTH_PIXELS,
        INITIAL_CELL_HEIGHT_PIXELS,
        TerminalEmulator.DEFAULT_TERMINAL_TRANSCRIPT_ROWS,
        EmulatorClient,
    )

    private val outputRelay = PtyOutputRelay(
        consume = { bytes -> emulator.append(bytes, bytes.size) },
        onScreenChanged = ::screenChanged,
    )

    fun setScreenChangedListener(listener: (() -> Unit)?) {
        onScreenChanged = listener
    }

    /** Called by the view whenever its pixel size or font metrics change. */
    fun resize(
        columns: Int,
        rows: Int,
        cellWidthPixels: Int = INITIAL_CELL_WIDTH_PIXELS,
        cellHeightPixels: Int = INITIAL_CELL_HEIGHT_PIXELS,
    ) {
        if (closed) return
        emulator.resize(columns, rows, cellWidthPixels, cellHeightPixels)
        if (sessionId == 0L) {
            startNative(rows, columns)
        } else {
            NativePtyBridge.resize(sessionId, rows, columns)
        }
        screenChanged()
    }

    /** Opens the PTY independently of TerminalView layout (needed by X11 sessions). */
    fun start() {
        if (!closed && sessionId == 0L) startNative(INITIAL_ROWS, INITIAL_COLUMNS)
    }

    fun isRunning(): Boolean = !closed && sessionId != 0L

    private fun startNative(rows: Int, columns: Int) {
        sessionId = openNativeSession(rows, columns, this)
    }

    override fun updateSize(
        columns: Int,
        rows: Int,
        cellWidthPixels: Int,
        cellHeightPixels: Int,
    ) = resize(columns, rows, cellWidthPixels, cellHeightPixels)

    fun close() {
        if (closed) return
        closed = true
        outputRelay.close()
        val id = sessionId
        sessionId = 0L
        if (id != 0L) NativePtyBridge.close(id)
        onScreenChanged = null
    }

    override fun write(data: ByteArray, offset: Int, count: Int) {
        val id = sessionId
        if (!closed && id != 0L && count > 0) {
            NativePtyBridge.write(id, data.copyOfRange(offset, offset + count))
        }
    }

    fun write(bytes: ByteArray) {
        write(bytes, 0, bytes.size)
    }

    override fun writeCodePoint(prependEscape: Boolean, codePoint: Int) {
        if (prependEscape) write("\u001b")
        write(String(Character.toChars(codePoint)))
    }

    override fun getEmulator(): TerminalEmulator = emulator

    // The upstream Java terminal API supplies a null old title on its first
    // OSC title update. Trierarch does not display titles yet, so ignore both
    // values while preserving that Java nullability boundary.
    override fun titleChanged(oldTitle: String?, newTitle: String?) = Unit

    override fun onCopyTextToClipboard(text: String) = clipboard.copy(text)

    override fun onPasteTextFromClipboard() {
        clipboard.paste()?.let(::write)
    }

    override fun onBell() = Unit

    override fun onColorsChanged() = screenChanged()

    override fun onSessionOutput(sessionId: Long, bytes: ByteArray) {
        if (!closed && sessionId == this.sessionId) {
            outputRelay.offer(bytes)
        }
    }

    override fun onSessionExited(sessionId: Long, exitCode: Int) {
        if (sessionId == this.sessionId) {
            this.sessionId = 0L
            screenChanged()
        }
    }

    private fun screenChanged() {
        onScreenChanged?.invoke()
    }

    private object EmulatorClient : TerminalSessionClient {
        override fun onTextChanged(changedSession: TerminalSession) = Unit
        override fun onTitleChanged(changedSession: TerminalSession) = Unit
        override fun onSessionFinished(finishedSession: TerminalSession) = Unit
        override fun onCopyTextToClipboard(session: TerminalSession, text: String) = Unit
        override fun onPasteTextFromClipboard(session: TerminalSession?) = Unit
        override fun onBell(session: TerminalSession) = Unit
        override fun onColorsChanged(session: TerminalSession) = Unit
        override fun onTerminalCursorStateChange(state: Boolean) = Unit
        override fun setTerminalShellPid(session: TerminalSession, pid: Int) = Unit
        override fun getTerminalCursorStyle() = TerminalEmulator.TERMINAL_CURSOR_STYLE_BLOCK
        override fun logError(tag: String, message: String) { Log.e(tag, message) }
        override fun logWarn(tag: String, message: String) { Log.w(tag, message) }
        override fun logInfo(tag: String, message: String) { Log.i(tag, message) }
        override fun logDebug(tag: String, message: String) { Log.d(tag, message) }
        override fun logVerbose(tag: String, message: String) { Log.v(tag, message) }
        override fun logStackTraceWithMessage(tag: String, message: String, exception: Exception) { Log.e(tag, message, exception) }
        override fun logStackTrace(tag: String, exception: Exception) { Log.e(tag, Log.getStackTraceString(exception)) }
    }

    private companion object {
        const val INITIAL_COLUMNS = 80
        const val INITIAL_ROWS = 24
        const val INITIAL_CELL_WIDTH_PIXELS = 1
        const val INITIAL_CELL_HEIGHT_PIXELS = 1
    }
}
