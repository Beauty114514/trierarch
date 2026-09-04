package app.trierarch.terminal

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import app.trierarch.config.ProfileStore
import app.trierarch.compat.GuestCompatibilityRuntime
import app.trierarch.runtime.InternalShellLaunchSpec
import app.trierarch.x11.X11Runtime
import app.trierarch.wayland.WaylandBridge
import app.trierarch.virgl.VirglHostController
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
        val virglRuntimeDirectory = if (profile.graphics.renderer == ProfileStore.GRAPHICS_VIRGL) {
            VirglHostController.start(app).absolutePath
        } else {
            ""
        }
        if (profile.display == ProfileStore.DISPLAY_WAYLAND) {
            check(WaylandBridge.start(app)) { "Unable to start Wayland host" }
        }
        session = NativePtySession(
            rootfsDirectory = profile.rootfs,
            shell = profile.shell,
            nativeLibraryDirectory = File(app.applicationInfo.nativeLibraryDir),
            cacheDirectory = app.cacheDir,
            x11SocketDirectory = x11SocketDirectory(profile.display),
            waylandRuntimeDirectory = waylandRuntimeDirectory(profile.display),
            virglRuntimeDirectory = virglRuntimeDirectory,
            udevCompatibilityLibrary = GuestCompatibilityRuntime.udevMonitorLibrary(
                app, profile.compatibility.enablesUdevMonitorShim,
            )?.absolutePath.orEmpty(),
            launchArgv = profile.launchArgv.orEmpty().toTypedArray(),
            graphicsEnvironment = profile.graphics.environment().toTypedArray(),
            clipboard = AndroidTerminalClipboard(app),
        ).also { next ->
            if (profile.display == ProfileStore.DISPLAY_X11 || profile.display == ProfileStore.DISPLAY_WAYLAND) {
                next.start()
            }
        }
    }

    fun restartChroot(profile: ProfileStore.ChrootProfile) {
        session.close()
        session = NativePtySession(
            chrootRootfs = profile.rootfs,
            shell = profile.shell,
            x11SocketDirectory = x11SocketDirectory(profile.display),
            launchArgv = profile.launchArgv.orEmpty().toTypedArray(),
            graphicsEnvironment = profile.graphics.environment().toTypedArray(),
            udevCompatibilityLibrary = GuestCompatibilityRuntime.udevMonitorLibrary(
                app, profile.compatibility.enablesUdevMonitorShim,
            )?.absolutePath.orEmpty(),
            clipboard = AndroidTerminalClipboard(app),
        ).also { next ->
            if (profile.display == ProfileStore.DISPLAY_X11 || profile.display == ProfileStore.DISPLAY_WAYLAND) {
                next.start()
            }
        }
    }

    fun restartDroidspaces(profile: ProfileStore.DroidspacesProfile) {
        session.close()
        val virglRuntimeDirectory = if (profile.graphics.renderer == ProfileStore.GRAPHICS_VIRGL) {
            VirglHostController.start(app).absolutePath
        } else {
            ""
        }
        if (profile.display == ProfileStore.DISPLAY_WAYLAND) {
            check(WaylandBridge.start(app)) { "Unable to start Wayland host" }
        }
        session = NativePtySession(
            droidspacesProfile = profile,
            x11SocketDirectory = x11SocketDirectory(profile.display),
            waylandRuntimeDirectory = waylandRuntimeDirectory(profile.display),
            virglRuntimeDirectory = virglRuntimeDirectory,
            udevCompatibilityLibrary = GuestCompatibilityRuntime.udevMonitorLibrary(
                app, profile.compatibility.enablesUdevMonitorShim,
            )?.absolutePath.orEmpty(),
            clipboard = AndroidTerminalClipboard(app),
        ).also { next ->
            if (profile.display == ProfileStore.DISPLAY_X11 || profile.display == ProfileStore.DISPLAY_WAYLAND) {
                next.start()
            }
        }
    }

    fun isRuntimeRunning(): Boolean = session.isRunning()

    /** Trierarch owns the active runtime session, including chroot and PRoot. */
    fun stopRuntime() {
        session.close()
        WaylandBridge.stop()
        VirglHostController.stop()
        session = createInternalShell()
    }

    override fun onCleared() {
        session.close()
    }

    private fun createInternalShell() = NativePtySession(
        launchSpec = InternalShellLaunchSpec.create(
            filesDirectory = app.filesDir,
            cacheDirectory = app.cacheDir,
            nativeLibraryDirectory = File(app.applicationInfo.nativeLibraryDir),
        ),
        clipboard = AndroidTerminalClipboard(app),
    )

    private fun x11SocketDirectory(display: String): String? =
        X11Runtime.socketDirectory(app).absolutePath.takeIf {
            display == ProfileStore.DISPLAY_X11
        }

    private fun waylandRuntimeDirectory(display: String): String? =
        File(app.filesDir, "wayland/runtime").also { it.mkdirs() }.absolutePath.takeIf {
            display == ProfileStore.DISPLAY_WAYLAND
        }
}
