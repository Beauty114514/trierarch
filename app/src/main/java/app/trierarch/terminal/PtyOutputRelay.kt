package app.trierarch.terminal

import android.os.Handler
import android.os.Looper
import java.util.ArrayDeque

/**
 * Moves one PTY session's output from its native callback thread to the main thread.
 *
 * Output is batched so a burst schedules at most one pending main-thread drain. The queue is
 * bounded to protect the process from an unresponsive UI; when full, the oldest unseen chunks
 * are discarded in favour of the newest terminal state.
 */
internal class PtyOutputRelay(
    private val consume: (ByteArray) -> Unit,
    private val onScreenChanged: () -> Unit,
) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val lock = Any()
    private val pendingChunks = ArrayDeque<ByteArray>()
    private var pendingBytes = 0
    private var drainScheduled = false
    private var closed = false

    fun offer(bytes: ByteArray) {
        if (bytes.isEmpty()) return

        var chunk = bytes.copyOf()
        var shouldSchedule = false
        synchronized(lock) {
            if (closed) return

            if (chunk.size > MAX_PENDING_BYTES) {
                chunk = chunk.copyOfRange(chunk.size - MAX_PENDING_BYTES, chunk.size)
            }
            while (pendingBytes + chunk.size > MAX_PENDING_BYTES && pendingChunks.isNotEmpty()) {
                pendingBytes -= pendingChunks.removeFirst().size
            }
            pendingChunks.addLast(chunk)
            pendingBytes += chunk.size

            if (!drainScheduled) {
                drainScheduled = true
                shouldSchedule = true
            }
        }
        if (shouldSchedule) mainHandler.post(drainRunnable)
    }

    fun close() {
        synchronized(lock) {
            closed = true
            pendingChunks.clear()
            pendingBytes = 0
            drainScheduled = false
        }
        mainHandler.removeCallbacks(drainRunnable)
    }

    private val drainRunnable = Runnable {
        val chunks = ArrayList<ByteArray>()
        synchronized(lock) {
            if (closed) return@Runnable
            while (pendingChunks.isNotEmpty()) {
                chunks += pendingChunks.removeFirst()
            }
            pendingBytes = 0
            drainScheduled = false
        }

        chunks.forEach(consume)
        if (chunks.isNotEmpty()) onScreenChanged()
    }

    private companion object {
        const val MAX_PENDING_BYTES = 1024 * 1024
    }
}
