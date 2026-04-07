package app.trierarch

import android.media.AudioManager
import android.os.Bundle
import android.view.KeyEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import app.trierarch.input.HardwareKeyEventPolicy
import app.trierarch.input.HardwareKeyboardRouter
import app.trierarch.input.InputRouteState
import app.trierarch.ui.AppScreen
import app.trierarch.ui.theme.TrierarchTheme

class MainActivity : ComponentActivity() {
    companion object {
        const val ACTION_OPEN_TERMINAL = "app.trierarch.action.OPEN_TERMINAL"
    }

    private val hardwareKeyboardRouter = HardwareKeyboardRouter()
    private var startInTerminal by mutableStateOf(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        volumeControlStream = AudioManager.STREAM_MUSIC
        startInTerminal = intent?.action == ACTION_OPEN_TERMINAL
        setContent {
            TrierarchTheme {
                AppScreen(startInTerminal = startInTerminal)
            }
        }
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        startInTerminal = intent.action == ACTION_OPEN_TERMINAL
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (hardwareKeyboardRouter.handleHardwareKeyboardEvent(event)) return true
        if (!InputRouteState.waylandVisible && HardwareKeyEventPolicy.isLikelyFromHardwareKeyboard(event)) {
            val tv = InputRouteState.shellTerminalView
            if (tv != null) {
                tv.requestFocus()
                if (tv.dispatchKeyEvent(event)) return true
            }
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (hardwareKeyboardRouter.handleHardwareKeyboardEvent(event)) return true
        if (!InputRouteState.waylandVisible && HardwareKeyEventPolicy.isLikelyFromHardwareKeyboard(event)) {
            val tv = InputRouteState.shellTerminalView
            if (tv != null) {
                tv.requestFocus()
                if (tv.dispatchKeyEvent(event)) return true
            }
        }
        return super.onKeyUp(keyCode, event)
    }

    override fun onKeyShortcut(keyCode: Int, event: KeyEvent): Boolean {
        if (hardwareKeyboardRouter.handleHardwareKeyboardEvent(event)) return true
        if (!InputRouteState.waylandVisible && HardwareKeyEventPolicy.isLikelyFromHardwareKeyboard(event)) {
            val tv = InputRouteState.shellTerminalView
            if (tv != null) {
                tv.requestFocus()
                if (tv.dispatchKeyShortcutEvent(event)) return true
            }
        }
        return super.onKeyShortcut(keyCode, event)
    }
}
