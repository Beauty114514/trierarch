package app.trierarch.x11

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Build
import android.os.IBinder
import android.view.ViewGroup
import android.widget.FrameLayout
import com.termux.x11.IX11Server
import com.termux.x11.LorieView
import com.termux.x11.X11ServerService

/** Connects one embedded Lorie SurfaceView to the isolated X11 server process. */
class X11HostController(context: Context) {
    private val appContext = context.applicationContext
    private var server: IX11Server? = null
    private var display: LorieView? = null
    private var binding = false
    private var retryCount = 0
    private var readyRetryCount = 0
    private var awaitingReady = false
    private var onReady: (() -> Unit)? = null
    private var onFailure: ((String) -> Unit)? = null

    /** Starts Lorie and reports only when its client connection and X0 socket are usable. */
    fun showIn(
        container: FrameLayout,
        onReady: () -> Unit,
        onFailure: (String) -> Unit,
    ) {
        check(Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            "The bundled Lorie X11 host requires Android 8.0 or newer"
        }
        val view = display ?: LorieView(container.context).also { display = it }
        if (view.parent == null) {
            container.addView(
                view,
                1,
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                ),
            )
        }
        view.visibility = android.view.View.VISIBLE
        // Lorie renders into SurfaceView's separate surface. Its ordinary View layer
        // must stay transparent or it covers that surface with a solid rectangle.
        view.setBackgroundColor(android.graphics.Color.TRANSPARENT)
        X11Runtime.prepareSocketDirectory(container.context)
        this.onReady = onReady
        this.onFailure = onFailure
        readyRetryCount = 0
        bindServerIfNeeded()
        connectWhenReady()
        awaitReady()
    }

    /** Leaves the server alive for a later X11 session, but returns the app to its terminal. */
    fun hide() {
        // A profile may be cancelled while Lorie is still creating its socket.
        // Do not let that stale readiness callback launch it afterwards.
        awaitingReady = false
        readyRetryCount = 0
        onReady = null
        onFailure = null
        display?.visibility = android.view.View.INVISIBLE
    }

    private fun bindServerIfNeeded() {
        if (binding || server != null) return
        binding = appContext.bindService(
            Intent(appContext, X11ServerService::class.java),
            connection,
            Context.BIND_AUTO_CREATE,
        )
        check(binding) { "Unable to bind the X11 server process" }
    }

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            server = IX11Server.Stub.asInterface(binder)
            retryCount = 0
            connectWhenReady()
        }

        override fun onServiceDisconnected(name: ComponentName) {
            server = null
        }
    }

    private fun connectWhenReady() {
        val view = display ?: return
        if (view.isConnected()) return
        val currentServer = server ?: return
        val connection = runCatching { currentServer.xConnection }.getOrNull()
        if (connection != null) {
            view.attachConnection(connection.detachFd())
            retryCount = 0
            awaitReady()
            return
        }
        // Xorg starts on its own service thread; wait only for that short startup window.
        if (retryCount++ < MAX_CONNECTION_RETRIES) {
            view.postDelayed(::connectWhenReady, CONNECTION_RETRY_MILLIS)
        }
    }

    private fun awaitReady() {
        if (awaitingReady || onReady == null) return
        awaitingReady = true
        display?.post(::checkReady)
    }

    private fun checkReady() {
        val view = display ?: return finishFailed("X11 display view is unavailable")
        // The socket is the runtime contract consumed by container backends.
        // Lorie's renderer may report its client connection later, so it must
        // not gate launching the container.
        if (X11Runtime.socketDirectory(appContext).resolve("X0").exists()) {
            awaitingReady = false
            readyRetryCount = 0
            onReady?.also { callback ->
                onReady = null
                onFailure = null
                callback()
            }
            return
        }
        if (++readyRetryCount > MAX_READY_RETRIES) {
            finishFailed("Timed out waiting for the Trierarch X11 server")
            return
        }
        view.postDelayed(::checkReady, CONNECTION_RETRY_MILLIS)
    }

    private fun finishFailed(message: String) {
        awaitingReady = false
        onFailure?.also { callback ->
            onReady = null
            onFailure = null
            callback(message)
        }
    }

    private companion object {
        const val CONNECTION_RETRY_MILLIS = 100L
        const val MAX_CONNECTION_RETRIES = 50
        const val MAX_READY_RETRIES = 50
    }
}
