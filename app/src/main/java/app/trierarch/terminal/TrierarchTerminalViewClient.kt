package app.trierarch.terminal

import android.content.Context
import android.util.Log
import android.util.TypedValue
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.inputmethod.InputMethodManager
import com.termux.view.DisplayableTermSession
import com.termux.view.TerminalView
import com.termux.view.TerminalViewClient

/** Default interaction policy for Trierarch's embedded terminal view. */
class TrierarchTerminalViewClient(
    private val terminalView: TerminalView,
    initialTextSizePx: Int,
) : TerminalViewClient {
    private var textSizePx = initialTextSizePx

    override fun onScale(scaleFactor: Float): Float {
        val metrics = terminalView.resources.displayMetrics
        val stepPx = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 1f, metrics).toInt().coerceAtLeast(1)
        val minPx = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 8f, metrics).toInt().coerceAtLeast(8)
        val maxPx = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 32f, metrics).toInt().coerceAtLeast(minPx + 1)

        return when {
            scaleFactor > 1.1f -> {
                textSizePx = (textSizePx + stepPx).coerceAtMost(maxPx)
                terminalView.setTextSize(textSizePx)
                1.0f
            }
            scaleFactor < 0.9f -> {
                textSizePx = (textSizePx - stepPx).coerceAtLeast(minPx)
                terminalView.setTextSize(textSizePx)
                1.0f
            }
            else -> scaleFactor
        }
    }

    override fun onSingleTapUp(event: MotionEvent) {
        val inputMethodManager = terminalView.context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        terminalView.post {
            terminalView.requestFocus()
            inputMethodManager.showSoftInput(terminalView, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    override fun shouldBackButtonBeMappedToEscape() = true

    override fun shouldEnforceCharBasedInput() = true

    override fun shouldUseCtrlSpaceWorkaround() = false

    override fun isTerminalViewSelected() = true

    override fun copyModeChanged(copyMode: Boolean) = Unit

    override fun onKeyDown(keyCode: Int, event: KeyEvent, session: DisplayableTermSession) = false

    override fun onKeyUp(keyCode: Int, event: KeyEvent) = false

    override fun onLongPress(event: MotionEvent) = false

    override fun readControlKey() = false

    override fun readAltKey() = false

    override fun readShiftKey() = false

    override fun readFnKey() = false

    override fun onCodePoint(codePoint: Int, ctrlDown: Boolean, session: DisplayableTermSession) = false

    override fun onEmulatorSet() = Unit

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

    override fun logStackTraceWithMessage(tag: String, message: String, exception: Exception) {
        Log.e(tag, message, exception)
    }

    override fun logStackTrace(tag: String, exception: Exception) {
        Log.e(tag, Log.getStackTraceString(exception))
    }
}
