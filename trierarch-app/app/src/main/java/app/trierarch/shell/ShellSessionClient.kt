package app.trierarch.shell

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.util.Log
import app.trierarch.NativeBridge
import com.termux.terminal.TerminalEmulator
import com.termux.terminal.TerminalSession
import com.termux.terminal.TerminalSessionClient
import com.termux.view.TerminalView

/** [TerminalSessionClient] for the emulator behind [app.trierarch.RustPtySession]. */
class ShellSessionClient(
    private val context: Context,
    private val terminalView: TerminalView,
    private val sessionId: Int
) : TerminalSessionClient {

    override fun onTextChanged(changedSession: TerminalSession) {
        terminalView.post { terminalView.invalidate() }
    }

    override fun onTitleChanged(changedSession: TerminalSession) {
        terminalView.postInvalidate()
    }

    override fun onSessionFinished(finishedSession: TerminalSession) {}

    override fun onCopyTextToClipboard(session: TerminalSession, text: String?) {
        if (text.isNullOrEmpty()) return
        val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        cm.setPrimaryClip(ClipData.newPlainText("", text))
    }

    override fun onPasteTextFromClipboard(session: TerminalSession?) {
        val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = cm.primaryClip?.getItemAt(0)?.coerceToText(context)?.toString() ?: return
        NativeBridge.writeInput(sessionId, clip.toByteArray(Charsets.UTF_8))
    }

    override fun onBell(session: TerminalSession) {}

    override fun onColorsChanged(session: TerminalSession) {
        terminalView.postInvalidate()
    }

    override fun onTerminalCursorStateChange(state: Boolean) {
        terminalView.postInvalidate()
    }

    override fun getTerminalCursorStyle(): Int = TerminalEmulator.TERMINAL_CURSOR_STYLE_BLOCK
    override fun logError(tag: String, message: String) {
        Log.e(tag, message)
    }

    override fun logWarn(tag: String, message: String) {
        Log.w(tag, message)
    }

    override fun logInfo(tag: String, message: String) {
        Log.i(tag, message)
    }

    override fun logDebug(tag: String, message: String) {
        Log.d(tag, message)
    }

    override fun logVerbose(tag: String, message: String) {
        Log.v(tag, message)
    }

    override fun logStackTraceWithMessage(tag: String, message: String, e: Exception) {
        Log.e(tag, message, e)
    }

    override fun logStackTrace(tag: String, e: Exception) {
        Log.e(tag, Log.getStackTraceString(e))
    }
}
