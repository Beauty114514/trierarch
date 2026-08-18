package app.trierarch.terminal

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import app.trierarch.config.ProfileStore
import app.trierarch.runtime.InternalShellLaunchSpec
import app.trierarch.x11.X11Runtime
import java.io.File

/** Owns the built-in app-internal shell independently of a terminal view instance. */
class DefaultTerminalViewModel(application: Application) : AndroidViewModel(application) {
    private val app = application

    var session: NativePtySession = createInternalShell()
        private set

    fun restartInternalShell() {
        session.close()
        session = createInternalShell()
    }

    fun restartProot(profile: ProfileStore.ProotProfile) {
        session.close()
        session = NativePtySession(
            rootfsDirectory = profile.rootfs,
            shell = profile.shell,
            nativeLibraryDirectory = File(app.applicationInfo.nativeLibraryDir),
            cacheDirectory = app.cacheDir,
            x11SocketDirectory = x11SocketDirectory(profile.display),
            launchArgv = profile.launchArgv.orEmpty().toTypedArray(),
            clipboard = AndroidTerminalClipboard(app),
        ).also { next -> if (profile.display == ProfileStore.DISPLAY_X11) next.start() }
    }

    fun restartChroot(profile: ProfileStore.ChrootProfile) {
        session.close()
        session = NativePtySession(
            chrootRootfs = profile.rootfs,
            shell = profile.shell,
            x11SocketDirectory = x11SocketDirectory(profile.display),
            launchArgv = profile.launchArgv.orEmpty().toTypedArray(),
            clipboard = AndroidTerminalClipboard(app),
        ).also { next -> if (profile.display == ProfileStore.DISPLAY_X11) next.start() }
    }

    fun restartDroidspaces(profile: ProfileStore.DroidspacesProfile) {
        session.close()
        session = NativePtySession(
            droidspacesProfile = profile,
            x11SocketDirectory = x11SocketDirectory(profile.display),
            clipboard = AndroidTerminalClipboard(app),
        ).also { next -> if (profile.display == ProfileStore.DISPLAY_X11) next.start() }
    }

    fun isRuntimeRunning(): Boolean = session.isRunning()

    /** Trierarch owns the active runtime session, including chroot and PRoot. */
    fun stopRuntime() {
        session.close()
        session = createInternalShell()
    }

    override fun onCleared() {
        session.close()
    }

    private fun createInternalShell() = NativePtySession(
        launchSpec = InternalShellLaunchSpec.create(app.filesDir, app.cacheDir),
        clipboard = AndroidTerminalClipboard(app),
    )

    private fun x11SocketDirectory(display: String): String? =
        X11Runtime.socketDirectory(app).absolutePath.takeIf {
            display == ProfileStore.DISPLAY_X11
        }
}
