package app.trierarch.compat

import android.content.Context
import java.io.File

/** Extracts a guest-glibc library from the APK; it is not an Android JNI library. */
object GuestCompatibilityRuntime {
    private const val ASSET_PATH = "compat/arm64-v8a/libtrierarch-udev-compat.so"
    private const val FILE_NAME = "libtrierarch-udev-compat.so"

    fun udevMonitorLibrary(context: Context, enabled: Boolean): File? {
        if (!enabled) return null
        val directory = File(context.filesDir, "compat/arm64-v8a")
        check(directory.isDirectory || directory.mkdirs()) {
            "Unable to create guest compatibility directory"
        }
        val destination = File(directory, FILE_NAME)
        if (!destination.isFile || destination.length() == 0L) {
            val temporary = File(directory, ".${FILE_NAME}.${System.nanoTime()}.tmp")
            context.assets.open(ASSET_PATH).use { input ->
                temporary.outputStream().use(input::copyTo)
            }
            check(temporary.renameTo(destination)) { "Unable to install guest compatibility library" }
        }
        return destination
    }
}
