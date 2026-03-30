package app.trierarch.wayland

import android.content.Context
import android.database.ContentObserver
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.Settings
import android.view.inputmethod.InputMethodManager
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import app.trierarch.input.SoftKeyboardView

/**
 * Keeps a hidden IME sink reliably visible across app focus changes and IME app switches.
 *
 * Contract:
 * - Call [setWanted] when the user explicitly requests the keyboard.
 * - Call [bindLifecycleOwner] from Compose so we can re-show on resume.
 * - The caller owns the view hierarchy; this helper owns only observers/runnables.
 */
internal class WaylandImeRecovery(
    private val context: Context,
    private val keyboardSinkViewProvider: () -> SoftKeyboardView?
) {
    private var wanted: Boolean = false
    private var lastShowSoftInputMs: Long = 0L
    private val mainHandler = Handler(Looper.getMainLooper())
    private var imeObserver: ContentObserver? = null
    private var pendingImeReshow: Runnable? = null
    private var lifecycleObserver: DefaultLifecycleObserver? = null
    private var boundLifecycleOwner: LifecycleOwner? = null

    private companion object {
        const val RESHOW_DEBOUNCE_MS = 250L
    }

    fun setWanted(wanted: Boolean) {
        this.wanted = wanted
    }

    fun bindLifecycleOwner(owner: LifecycleOwner?) {
        if (owner == boundLifecycleOwner) return

        boundLifecycleOwner?.let { prev ->
            lifecycleObserver?.let { obs -> prev.lifecycle.removeObserver(obs) }
        }
        boundLifecycleOwner = owner

        if (lifecycleObserver == null) {
            lifecycleObserver = object : DefaultLifecycleObserver {
                override fun onResume(owner: LifecycleOwner) {
                    ensureVisible()
                }
            }
        }
        if (owner != null) {
            owner.lifecycle.addObserver(lifecycleObserver!!)
        }
    }

    fun onAttachedToWindow() {
        if (imeObserver != null) return

        imeObserver = object : ContentObserver(mainHandler) {
            override fun onChange(selfChange: Boolean) {
                if (!wanted) return
                // Default IME changed. Let the switch settle before requesting show again.
                pendingImeReshow?.let { mainHandler.removeCallbacks(it) }
                pendingImeReshow = Runnable { ensureVisible() }
                mainHandler.postDelayed(pendingImeReshow!!, 120)
            }
        }
        try {
            context.contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.DEFAULT_INPUT_METHOD),
                false,
                imeObserver!!
            )
        } catch (_: Throwable) {
        }
    }

    fun onDetachedFromWindow() {
        bindLifecycleOwner(null)

        pendingImeReshow?.let { mainHandler.removeCallbacks(it) }
        pendingImeReshow = null

        imeObserver?.let { obs ->
            try {
                context.contentResolver.unregisterContentObserver(obs)
            } catch (_: Throwable) {
            }
        }
        imeObserver = null
    }

    fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        if (hasWindowFocus) ensureVisible()
    }

    fun ensureVisible() {
        if (!wanted) return
        val kv = keyboardSinkViewProvider() ?: return

        val now = SystemClock.uptimeMillis()
        if (now - lastShowSoftInputMs < RESHOW_DEBOUNCE_MS) return
        lastShowSoftInputMs = now

        fun attempt(delayMs: Long) {
            val r = Runnable {
                pendingImeReshow = null
                try {
                    kv.requestFocus()
                    val imm = kv.context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
                    if (imm != null) {
                        imm.restartInput(kv)
                        val shown = imm.showSoftInput(kv, InputMethodManager.SHOW_IMPLICIT)
                        if (!shown) attempt(180)
                    }
                } catch (_: Throwable) {
                }
            }
            pendingImeReshow = r
            if (delayMs <= 0) mainHandler.post(r) else mainHandler.postDelayed(r, delayMs)
        }

        attempt(0)
    }
}

