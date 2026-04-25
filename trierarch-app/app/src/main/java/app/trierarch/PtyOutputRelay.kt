package app.trierarch

import android.util.Log
import android.os.Handler
import android.os.Looper
import com.termux.view.TerminalView
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.atomic.AtomicInteger

/**
 * Delivers PTY output from native code to the UI [TerminalEmulator] on the main thread.
 * Each [sessionId] has its own byte queue; only the bound session drains into the view.
 */
object PtyOutputRelay {
    /** Primary tag (matches common `adb logcat *:S TrierarchContainerIO:I`). */
    private const val TAG_IO = "TrierarchContainerIO"
    private const val TAG_IO_MIRROR = "TrierarchDisplayIO"
    private const val MAX_PENDING_CHUNKS = 256
    private const val MAX_LOG_CHARS = 800
    /** Throttle; thousands of [Log.i] (even from reader thread) can still stall the process / system log. */
    private const val IO_LOG_MIN_INTERVAL_MS = 500L

    private val outLogLastTime = ConcurrentHashMap<Int, Long>()
    private val outLogSuppressed = ConcurrentHashMap<Int, AtomicInteger>()

    private val mainHandler = Handler(Looper.getMainLooper())
    private val pendingBySession = ConcurrentHashMap<Int, ConcurrentLinkedQueue<ByteArray>>()
    private val logEnabledBySession = ConcurrentHashMap<Int, Boolean>()
    private var boundSession: RustPtySession? = null
    private var terminalView: TerminalView? = null

    /** Coalesces many PTY chunks into one main-thread drain + invalidate (avoids ANR when unbound sessions flood). */
    private val flushBoundSessionRunnable = Runnable {
        val sid = boundSession?.sessionId ?: return@Runnable
        drainPendingFor(sid)
        terminalView?.invalidate()
    }

    private fun queueFor(sessionId: Int): ConcurrentLinkedQueue<ByteArray> =
        pendingBySession.computeIfAbsent(sessionId) { ConcurrentLinkedQueue() }

    fun setSessionIoLoggingEnabled(sessionId: Int, enabled: Boolean) {
        if (enabled) {
            logEnabledBySession[sessionId] = true
        } else {
            logEnabledBySession.remove(sessionId)
            outLogLastTime.remove(sessionId)
            outLogSuppressed.remove(sessionId)
        }
    }

    fun logInjectedInput(sessionId: Int, label: String, bytes: ByteArray) {
        if (logEnabledBySession[sessionId] != true) return
        // IN is a handful of lines per transition; no throttle needed
        val s = runCatching { bytes.toString(Charsets.UTF_8) }.getOrNull() ?: "<non-utf8>"
        val line = "IN[s=$sessionId][$label]: ${truncateForLog(s)}"
        Log.i(TAG_IO, line)
        Log.i(TAG_IO_MIRROR, line)
    }

    @JvmStatic
    fun onPtyOutputChunk(sessionId: Int, data: ByteArray) {
        if (data.isEmpty()) return
        if (logEnabledBySession[sessionId] == true) {
            val now = android.os.SystemClock.uptimeMillis()
            val last = outLogLastTime[sessionId] ?: 0L
            if (now - last < IO_LOG_MIN_INTERVAL_MS) {
                outLogSuppressed.computeIfAbsent(sessionId) { AtomicInteger(0) }.incrementAndGet()
            } else {
                val sup = outLogSuppressed[sessionId]?.getAndSet(0) ?: 0
                val s = runCatching { data.toString(Charsets.UTF_8) }.getOrNull() ?: "<non-utf8>"
                var line = "OUT[s=$sessionId]: ${truncateForLog(s)}"
                if (sup > 0) line += " (suppressed $sup chunk(s) in ${IO_LOG_MIN_INTERVAL_MS}ms window)"
                Log.i(TAG_IO, line)
                Log.i(TAG_IO_MIRROR, line)
                outLogLastTime[sessionId] = now
            }
        }
        // Queue from native reader thread — do not post one Runnable per chunk unless this session
        // is shown in TerminalView; headless X11 injectors (e.g. xfce) would fill the main looper and ANR.
        val q = queueFor(sessionId)
        q.add(data)
        while (q.size > MAX_PENDING_CHUNKS) {
            q.poll()
        }
        if (boundSession?.sessionId == sessionId) {
            mainHandler.removeCallbacks(flushBoundSessionRunnable)
            mainHandler.post(flushBoundSessionRunnable)
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

    private fun truncateForLog(s: String): String {
        if (s.length <= MAX_LOG_CHARS) return s
        return s.take(MAX_LOG_CHARS) + "…(truncated, total=${s.length})"
    }
}
