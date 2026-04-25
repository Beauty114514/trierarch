package app.trierarch.ui.runtime

import kotlinx.coroutines.delay

object WaylandVisibilityCoordinator {
    /**
     * Waits until a Wayland desktop client is connected (or the caller cancels by returning false from [isStillPending]).
     *
     * @return true if a client became ready while pending; false otherwise.
     */
    suspend fun waitUntilDesktopClientReady(
        isStillPending: () -> Boolean,
        hasActiveClients: () -> Boolean,
        onReady: () -> Unit,
    ): Boolean {
        while (isStillPending()) {
            val ready = runCatching { hasActiveClients() }.getOrElse { false }
            if (ready) {
                // Small extra delay to avoid showing a black frame on first switch.
                delay(350)
                if (isStillPending()) {
                    onReady()
                    return true
                }
                return false
            }
            delay(100)
        }
        return false
    }
}

