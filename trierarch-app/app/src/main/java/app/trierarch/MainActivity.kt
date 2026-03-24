package app.trierarch

import android.os.Bundle
import android.view.KeyEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import app.trierarch.input.HardwareKeyboardRouter
import app.trierarch.ui.AppScreen
import app.trierarch.ui.theme.TrierarchTheme

class MainActivity : ComponentActivity() {
    private val hardwareKeyboardRouter = HardwareKeyboardRouter()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            TrierarchTheme {
                AppScreen()
            }
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (hardwareKeyboardRouter.handleHardwareKeyboardEvent(event)) return true
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchKeyShortcutEvent(event: KeyEvent): Boolean {
        if (hardwareKeyboardRouter.handleHardwareKeyboardEvent(event)) return true
        return super.dispatchKeyShortcutEvent(event)
    }
}
