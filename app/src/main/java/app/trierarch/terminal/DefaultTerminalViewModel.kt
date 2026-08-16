package app.trierarch.terminal

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import app.trierarch.config.ProfileStore
import app.trierarch.runtime.InternalShellLaunchSpec
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
            clipboard = AndroidTerminalClipboard(app),
        )
    }

    fun restartChroot(profile: ProfileStore.ChrootProfile) {
        session.close()
        session = NativePtySession(
            chrootRootfs = profile.rootfs,
            shell = profile.shell,
            clipboard = AndroidTerminalClipboard(app),
        )
    }

    fun restartDroidspaces(profile: ProfileStore.DroidspacesProfile) {
        session.close()
        session = NativePtySession(
            droidspacesProfile = profile,
            clipboard = AndroidTerminalClipboard(app),
        )
    }

    override fun onCleared() {
        session.close()
    }

    private fun createInternalShell() = NativePtySession(
        launchSpec = InternalShellLaunchSpec.create(app.filesDir, app.cacheDir),
        clipboard = AndroidTerminalClipboard(app),
    )
}
