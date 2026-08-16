package app.trierarch

import android.os.Bundle
import android.util.TypedValue
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.viewModels
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import app.trierarch.config.ConfigBookOverlay
import app.trierarch.terminal.DefaultTerminalViewModel
import app.trierarch.terminal.TrierarchTerminalViewClient
import app.trierarch.ui.FloatingMenuOrbView
import com.termux.view.TerminalView

/** The first Trierarch profile: a shell using the app's private files directory as its home. */
class MainActivity : AppCompatActivity() {
    private val terminalViewModel: DefaultTerminalViewModel by viewModels()
    private var terminalView: TerminalView? = null
    private var configBook: ConfigBookOverlay? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.statusBars())
        }
        window.statusBarColor = android.graphics.Color.BLACK
        window.navigationBarColor = android.graphics.Color.BLACK

        val terminalView = TerminalView(this, null)
        this.terminalView = terminalView
        terminalView.isFocusable = true
        terminalView.isFocusableInTouchMode = true
        terminalView.keepScreenOn = true
        terminalView.setBackgroundColor(android.graphics.Color.BLACK)
        val initialTextSizePx = TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_SP,
            14f,
            resources.displayMetrics,
        ).toInt().coerceAtLeast(1)
        terminalView.setTextSize(initialTextSizePx)
        terminalView.setTerminalViewClient(
            TrierarchTerminalViewClient(terminalView, initialTextSizePx),
        )
        val terminalContainer = FrameLayout(this).apply {
            addView(
                terminalView,
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                ),
            )
        }
        val menuOrb = FloatingMenuOrbView(
            context = this,
            preferences = getSharedPreferences("trierarch-ui", MODE_PRIVATE),
            onClick = {
                if (configBook == null) {
                    configBook = ConfigBookOverlay(
                        context = this,
                        onDismiss = { configBook = null },
                        onStartInternalShell = {
                            terminalViewModel.restartInternalShell()
                            attachTerminalSession()
                        },
                        onStartProot = { profile ->
                            terminalViewModel.restartProot(profile)
                            attachTerminalSession()
                        },
                        onStartChroot = { profile ->
                            terminalViewModel.restartChroot(profile)
                            attachTerminalSession()
                        },
                        onStartDroidspaces = { profile ->
                            terminalViewModel.restartDroidspaces(profile)
                            attachTerminalSession()
                        },
                    )
                    terminalContainer.addView(configBook)
                }
            },
        )
        terminalContainer.addView(menuOrb)
        ViewCompat.setOnApplyWindowInsetsListener(terminalContainer) { _, insets ->
            val imeBottom = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom
            menuOrb.setImeBottomInset(imeBottom)
            insets
        }
        setContentView(terminalContainer)
        ViewCompat.requestApplyInsets(terminalContainer)

        attachTerminalSession()
    }

    private fun attachTerminalSession() {
        val view = terminalView ?: return
        val session = terminalViewModel.session
        view.attachSession(session)
        session.setScreenChangedListener(view::onScreenUpdated)
        view.post(view::updateSize)
    }

    override fun onBackPressed() {
        configBook?.let { book ->
            (book.parent as? ViewGroup)?.removeView(book)
            configBook = null
        } ?: super.onBackPressed()
    }

}
