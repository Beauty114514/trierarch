package app.trierarch.virgl

import android.content.Context
import app.trierarch.nativebridge.NativePtyBridge
import java.io.File
import java.io.FileOutputStream

/**
 * Deploys and supervises the Android-side half of the VirGL vtest transport.
 *
 * The server is an APK asset because it is not a JNI library. Android mounts
 * the app's files directory noexec, therefore native code invokes it through
 * the platform linker after this class has copied its libraries beside it.
 */
object VirglHostController {
    private const val ASSET_ROOT = "virgl/arm64-v8a"
    private val angleLibraries = listOf(
        "libEGL_angle.so",
        "libGLESv1_CM_angle.so",
        "libGLESv2_angle.so",
        "libfeature_support_angle.so",
    )

    @Synchronized
    fun start(context: Context): File {
        val payload = File(context.filesDir, "virgl")
        extractPayload(context, payload)
        val runtime = File(context.filesDir, "virgl-run").apply { mkdirs() }
        check(runtime.isDirectory) { "Unable to create the VirGL runtime directory" }
        NativePtyBridge.startVirglHost(
            runtime.absolutePath,
            payload.absolutePath,
            context.applicationInfo.nativeLibraryDir,
        )
        return runtime
    }

    @Synchronized
    fun stop() = NativePtyBridge.stopVirglHost()

    private fun extractPayload(context: Context, payload: File) {
        copyAsset(context, "$ASSET_ROOT/virgl_test_server_android", File(payload, "bin/virgl_test_server_android"))
        copyAsset(context, "$ASSET_ROOT/virgl_render_server", File(payload, "bin/virgl_render_server"))
        copyAsset(context, "$ASSET_ROOT/libvirglrenderer.so", File(payload, "lib/libvirglrenderer.so"))
        copyAsset(context, "$ASSET_ROOT/libepoxy.so", File(payload, "lib/libepoxy.so"))
        angleLibraries.forEach { library ->
            copyAsset(context, "$ASSET_ROOT/angle/vulkan/$library", File(payload, "angle/vulkan/$library"))
        }
    }

    private fun copyAsset(context: Context, assetPath: String, destination: File) {
        destination.parentFile?.mkdirs()
        context.assets.open(assetPath).use { input ->
            FileOutputStream(destination).use { output -> input.copyTo(output) }
        }
    }
}
