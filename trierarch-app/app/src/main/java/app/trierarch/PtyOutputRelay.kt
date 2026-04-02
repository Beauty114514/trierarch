package app.trierarch

import android.os.Handler
import android.os.Looper
import com.termux.view.TerminalView
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue

/**
 * Delivers PTY output from native code to the UI [TerminalEmulator] on the main thread.
 * Each [sessionId] has its own byte queue; only the bound session drains into the view.
 */
object PtyOutputRelay {
    private const val MAX_PENDING_CHUNKS = 256

    private val mainHandler = Handler(Looper.getMainLooper())
    private val pendingBySession = ConcurrentHashMap<Int, ConcurrentLinkedQueue<ByteArray>>()
    private var boundSession: RustPtySession? = null
    private var terminalView: TerminalView? = null

    private fun queueFor(sessionId: Int): ConcurrentLinkedQueue<ByteArray> =
        pendingBySession.computeIfAbsent(sessionId) { ConcurrentLinkedQueue() }

    @JvmStatic
    fun onPtyOutputChunk(sessionId: Int, data: ByteArray) {
        if (data.isEmpty()) return
        mainHandler.post {
            val q = queueFor(sessionId)
            q.add(data)
            while (q.size > MAX_PENDING_CHUNKS) {
                q.poll()
            }
            if (boundSession?.sessionId == sessionId) {
                drainPendingFor(sessionId)
                terminalView?.invalidate()
            }
        }
    }

    fun bind(session: RustPtySession, view: TerminalView) {
        boundSession = session
        terminalView = view
        val flush = {
            drainPendingFor(session.sessionId)
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
    }

    fun discardSessionQueue(sessionId: Int) {
        pendingBySession.remove(sessionId)
    }

    private fun drainPendingFor(sessionId: Int) {
        val em = boundSession?.takeIf { it.sessionId == sessionId }?.emulatorOrNull() ?: return
        val q = pendingBySession[sessionId] ?: return
        while (true) {
            val b = q.poll() ?: break
            em.append(b, b.size)
        }
    }
}
