package app.trierarch

import android.os.Handler
import android.os.Looper
import com.termux.view.TerminalView
import java.util.concurrent.ConcurrentLinkedQueue

/**
 * Delivers PTY output from [NativeBridge] to the UI [TerminalEmulator] on the main thread.
 * Bytes are queued until [bind] attaches a session, then consumed in order.
 */
object PtyOutputRelay {
    private const val MAX_PENDING_CHUNKS = 256

    private val mainHandler = Handler(Looper.getMainLooper())
    private val pending = ConcurrentLinkedQueue<ByteArray>()
    private var boundSession: RustPtySession? = null
    private var terminalView: TerminalView? = null

    @JvmStatic
    fun onPtyOutputChunk(data: ByteArray) {
        if (data.isEmpty()) return
        mainHandler.post {
            pending.add(data)
            while (pending.size > MAX_PENDING_CHUNKS) {
                pending.poll()
            }
            val em = boundSession?.emulatorOrNull()
            if (em != null) {
                drainPending()
                terminalView?.invalidate()
            }
        }
    }

    fun bind(session: RustPtySession, view: TerminalView) {
        boundSession = session
        terminalView = view
        val flush = {
            drainPending()
            view.invalidate()
        }
        if (Looper.myLooper() == mainHandler.looper) {
            flush()
        } else {
            mainHandler.post(flush)
        }
    }

    fun unbind() {
        boundSession = null
        terminalView = null
        pending.clear()
    }

    private fun drainPending() {
        val em = boundSession?.emulatorOrNull() ?: return
        while (true) {
            val b = pending.poll() ?: break
            em.append(b, b.size)
        }
    }
}
