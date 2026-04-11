package app.trierarch

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.util.Log
import com.termux.terminal.TerminalEmulator
import com.termux.terminal.TerminalOutput
import com.termux.terminal.TerminalSessionClient
import com.termux.view.DisplayableTermSession
import com.termux.view.TerminalView

/**
 * Proot PTY ([NativeBridge]) wired into [TerminalEmulator] without the default subprocess spawn.
 */
class RustPtySession(
    private val context: Context,
    sessionClient: TerminalSessionClient,
    private val terminalView: TerminalView,
    val sessionId: Int
) : TerminalOutput(), DisplayableTermSession {

    private val sessionClient: TerminalSessionClient = sessionClient
    private var emulator: TerminalEmulator? = null
    private val utf8InputBuffer = ByteArray(5)
    private var didAppendWelcomeBanner: Boolean = false

    private var appliedPtyRows: Int = -1
    private var appliedPtyCols: Int = -1

    private fun syncPtyKernelWindowSize(rows: Int, cols: Int) {
        if (rows == appliedPtyRows && cols == appliedPtyCols) return
        appliedPtyRows = rows
        appliedPtyCols = cols
        NativeBridge.setPtyWindowSize(sessionId, rows, cols)
    }

    override fun updateSize(columns: Int, rows: Int) {
        if (!NativeBridge.spawnSession(sessionId, rows, columns)) {
            Log.e(TAG, "spawnSession failed ($sessionId)")
            return
        }
        syncPtyKernelWindowSize(rows, columns)
        if (emulator == null) {
            emulator = TerminalEmulator(
                this,
                columns,
                rows,
                TerminalEmulator.DEFAULT_TERMINAL_TRANSCRIPT_ROWS,
                sessionClient
            )
            if (!didAppendWelcomeBanner) {
                didAppendWelcomeBanner = true
                emulator!!.append(WELCOME_LINE, WELCOME_LINE.size)
            }
        } else {
            emulator!!.resize(columns, rows)
        }
        PtyOutputRelay.bind(this, terminalView)
    }

    override fun getEmulator(): TerminalEmulator = emulator
        ?: throw IllegalStateException("Terminal emulator not initialized (call updateSize first)")

    fun emulatorOrNull(): TerminalEmulator? = emulator

    override fun write(data: ByteArray, offset: Int, count: Int) {
        if (count <= 0) return
        NativeBridge.writeInput(sessionId, data.copyOfRange(offset, offset + count))
    }

    override fun writeCodePoint(prependEscape: Boolean, codePoint: Int) {
        if (codePoint > 1114111 || codePoint in 0xD800..0xDFFF) {
            throw IllegalArgumentException("Invalid code point: $codePoint")
        }
        var pos = 0
        if (prependEscape) utf8InputBuffer[pos++] = 27
        when {
            codePoint <= 0b1111111 -> utf8InputBuffer[pos++] = codePoint.toByte()
            codePoint <= 0b11111111111 -> {
                utf8InputBuffer[pos++] = (0b11000000 or (codePoint shr 6)).toByte()
                utf8InputBuffer[pos++] = (0b10000000 or (codePoint and 0b111111)).toByte()
            }
            codePoint <= 0b1111111111111111 -> {
                utf8InputBuffer[pos++] = (0b11100000 or (codePoint shr 12)).toByte()
                utf8InputBuffer[pos++] = (0b10000000 or ((codePoint shr 6) and 0b111111)).toByte()
                utf8InputBuffer[pos++] = (0b10000000 or (codePoint and 0b111111)).toByte()
            }
            else -> {
                utf8InputBuffer[pos++] = (0b11110000 or (codePoint shr 18)).toByte()
                utf8InputBuffer[pos++] = (0b10000000 or ((codePoint shr 12) and 0b111111)).toByte()
                utf8InputBuffer[pos++] = (0b10000000 or ((codePoint shr 6) and 0b111111)).toByte()
                utf8InputBuffer[pos++] = (0b10000000 or (codePoint and 0b111111)).toByte()
            }
        }
        write(utf8InputBuffer, 0, pos)
    }

    override fun titleChanged(oldTitle: String?, newTitle: String?) {
        terminalView.postInvalidate()
    }

    override fun onCopyTextToClipboard(text: String?) {
        if (text.isNullOrEmpty()) return
        val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        cm.setPrimaryClip(ClipData.newPlainText("", text))
    }

    override fun onPasteTextFromClipboard() {
        val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = cm.primaryClip?.getItemAt(0)?.coerceToText(context)?.toString() ?: return
        NativeBridge.writeInput(sessionId, clip.toByteArray(Charsets.UTF_8))
    }

    override fun onBell() {
        // Optional: system beep; keep quiet on mobile.
    }

    override fun onColorsChanged() {
        terminalView.postInvalidate()
    }

    private companion object {
        private const val TAG = "RustPtySession"
        // This terminal treats LF as line-down only; without CR the cursor stays in the same column.
        private val WELCOME_LINE =
            "\u001b[34mWelcome to Trierarch!\u001b[0m\n\r".toByteArray(Charsets.UTF_8)
    }
}
