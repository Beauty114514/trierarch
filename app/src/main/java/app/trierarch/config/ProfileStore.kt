package app.trierarch.config

import android.content.Context
import org.tomlj.Toml
import org.tomlj.TomlParseResult
import java.io.File

/** Direct filesystem access for profile documents; no index or database exists. */
class ProfileStore(context: Context) {
    private val directory = File(context.filesDir, "config/profiles")

    fun list(): List<File> = directory
        .listFiles { file -> file.isFile && file.extension == EXTENSION }
        ?.sortedBy { it.name }
        .orEmpty()

    fun read(file: File): String = file.readText()

    fun save(text: String, replacing: File? = null): File {
        val id = validate(text)
        check(directory.isDirectory || directory.mkdirs()) {
            "Unable to create profile directory"
        }
        val destination = File(directory, "$id.$EXTENSION")
        require(destination == replacing || !destination.exists()) {
            "A profile named '$id' already exists"
        }

        val temporary = File(directory, ".${id}.${System.nanoTime()}.tmp")
        temporary.writeText(text)
        check(temporary.renameTo(destination)) { "Unable to save profile '$id'" }
        if (replacing != null && replacing != destination && replacing.exists()) {
            check(replacing.delete()) { "Saved profile but could not remove its former file" }
        }
        return destination
    }

    fun delete(file: File) {
        require(file.parentFile == directory) { "Profile is outside the profile directory" }
        check(file.delete()) { "Unable to delete ${file.name}" }
    }

    /** Validates TOML syntax plus the stable filename identity required by the book. */
    fun validate(text: String): String {
        val parsed = Toml.parse(text)
        require(!parsed.hasErrors()) {
            parsed.errors().joinToString("\n") { error -> error.toString() }
        }
        val id = parsed.getString("id")?.trim().orEmpty()
        require(ID_PATTERN.matches(id)) {
            "id must contain only lowercase letters, digits, and hyphens"
        }
        val runtime = parsed.getString("runtime")?.trim().orEmpty()
        require(runtime.isNotEmpty()) { "runtime is required" }
        require(parsed.getString("name")?.trim().isNullOrEmpty().not()) { "name is required" }
        val display = parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE }
        require(display == DISPLAY_NONE || display == DISPLAY_X11 || display == DISPLAY_WAYLAND) {
            "display.type must be '$DISPLAY_NONE', '$DISPLAY_X11', or '$DISPLAY_WAYLAND'"
        }
        val graphics = graphicsProfile(parsed, display)
        if (graphics.renderer == GRAPHICS_VIRGL) {
            require(runtime == RUNTIME_DROIDSPACES) {
                "graphics.renderer '$GRAPHICS_VIRGL' is currently supported only by droidspaces"
            }
            require(display != DISPLAY_NONE) {
                "graphics.renderer '$GRAPHICS_VIRGL' requires display.type 'x11' or 'wayland'"
            }
        }
        launchArgv(parsed)
        if (runtime == RUNTIME_PROOT || runtime == RUNTIME_CHROOT) {
            val rootfs = parsed.getString("rootfs")?.trim().orEmpty()
            require(rootfs.isNotEmpty()) {
                "rootfs is required for this runtime"
            }
            require(rootfs.startsWith('/')) { "rootfs must be an absolute path" }
            val shell = parsed.getString("shell")?.trim().orEmpty()
            require(shell.startsWith('/') && shell.split('/').none { it == ".." }) {
                "shell must be an absolute path inside the rootfs"
            }
        }
        if (runtime == RUNTIME_DROIDSPACES) {
            require(parsed.getString("container")?.trim().isNullOrEmpty().not()) {
                "container is required"
            }
        }
        return id
    }

    fun runtime(text: String): String = Toml.parse(text).getString("runtime")?.trim().orEmpty()

    /** The display is optional; omitted means the terminal-only default. */
    fun display(text: String): String = Toml.parse(text)
        .getString("display.type")
        ?.trim()
        .orEmpty()
        .ifEmpty { DISPLAY_NONE }

    /** Reads the minimal, explicit contract for one PRoot session. */
    fun prootProfile(text: String): ProotProfile {
        val parsed = parse(text)
        require(parsed.getString("runtime")?.trim() == RUNTIME_PROOT) {
            "runtime must be '$RUNTIME_PROOT'"
        }
        val rootfsPath = required(parsed.getString("rootfs"), "rootfs")
        val shell = required(parsed.getString("shell"), "shell")
        require(shell.startsWith('/') && shell.split('/').none { it == ".." }) {
            "shell must be an absolute path inside the rootfs"
        }
        val rootfs = File(rootfsPath)
        require(rootfs.isDirectory) { "rootfs is not an accessible directory: $rootfsPath" }
        require(File(rootfs, shell.removePrefix("/")).isFile) {
            "configured shell does not exist in rootfs: $shell"
        }
        return ProotProfile(
            id = required(parsed.getString("id"), "id"),
            name = required(parsed.getString("name"), "name"),
            rootfs = rootfs,
            shell = shell,
            display = parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE },
            launchArgv = launchArgv(parsed),
            graphics = graphicsProfile(parsed, parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE }),
        )
    }

    /**
     * A chroot rootfs is checked only after elevation: its host directory may
     * intentionally be unreadable by the ordinary Android app UID.
     */
    fun chrootProfile(text: String): ChrootProfile {
        val parsed = parse(text)
        require(parsed.getString("runtime")?.trim() == RUNTIME_CHROOT) {
            "runtime must be '$RUNTIME_CHROOT'"
        }
        val rootfs = required(parsed.getString("rootfs"), "rootfs")
        val shell = required(parsed.getString("shell"), "shell")
        require(rootfs.startsWith('/')) { "rootfs must be an absolute path" }
        require(shell.startsWith('/') && shell.split('/').none { it == ".." }) {
            "shell must be an absolute path inside the rootfs"
        }
        return ChrootProfile(
            id = required(parsed.getString("id"), "id"),
            name = required(parsed.getString("name"), "name"),
            rootfs = rootfs,
            shell = shell,
            display = parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE },
            launchArgv = launchArgv(parsed),
            graphics = graphicsProfile(parsed, parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE }),
        )
    }

    /** A container DroidSpaces has deployed; Trierarch owns this profile's session start. */
    fun droidspacesProfile(text: String): DroidspacesProfile {
        val parsed = parse(text)
        require(parsed.getString("runtime")?.trim() == RUNTIME_DROIDSPACES) {
            "runtime must be '$RUNTIME_DROIDSPACES'"
        }
        return DroidspacesProfile(
            id = required(parsed.getString("id"), "id"),
            name = required(parsed.getString("name"), "name"),
            container = required(parsed.getString("container"), "container"),
            user = parsed.getString("user")?.trim().takeUnless { it.isNullOrEmpty() } ?: "root",
            display = parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE },
            launchArgv = launchArgv(parsed),
            graphics = graphicsProfile(parsed, parsed.getString("display.type")?.trim().orEmpty().ifEmpty { DISPLAY_NONE }),
        )
    }

    private fun parse(text: String) = Toml.parse(text).also { parsed ->
        require(!parsed.hasErrors()) {
            parsed.errors().joinToString("\n") { error -> error.toString() }
        }
    }

    private fun required(value: String?, key: String): String = value?.trim().takeUnless { it.isNullOrEmpty() }
        ?: throw IllegalArgumentException("$key is required")

    /**
     * Rendering policy is deliberately small for now.  The app resolves it to
     * concrete environment variables before crossing the native boundary, so
     * every runtime receives the same guest-facing contract.
     */
    private fun graphicsProfile(parsed: TomlParseResult, display: String): GraphicsProfile {
        val renderer = parsed.getString("graphics.renderer")?.trim().orEmpty().ifEmpty { GRAPHICS_AUTO }
        require(renderer == GRAPHICS_AUTO || renderer == GRAPHICS_LLVMPIPE || renderer == GRAPHICS_VIRGL) {
            "graphics.renderer must be '$GRAPHICS_AUTO', '$GRAPHICS_LLVMPIPE', or '$GRAPHICS_VIRGL'"
        }
        val qtQuickBackend = parsed.getString("graphics.qt_quick_backend")?.trim().orEmpty()
            .ifEmpty {
                if (display == DISPLAY_NONE || renderer == GRAPHICS_VIRGL) GRAPHICS_AUTO else QT_QUICK_SOFTWARE
            }
        require(qtQuickBackend == GRAPHICS_AUTO || qtQuickBackend == QT_QUICK_SOFTWARE) {
            "graphics.qt_quick_backend must be '$GRAPHICS_AUTO' or '$QT_QUICK_SOFTWARE'"
        }
        return GraphicsProfile(renderer, qtQuickBackend)
    }

    /** Optional direct program invocation; absent keeps the runtime's interactive-shell default. */
    private fun launchArgv(parsed: TomlParseResult): List<String>? {
        if (!parsed.contains("launch.argv")) return null
        require(parsed.isArray("launch.argv")) { "launch.argv must be an array of strings" }
        val array = checkNotNull(parsed.getArray("launch.argv"))
        require(array.size() > 0) { "launch.argv must not be empty" }
        return (0 until array.size()).map { index ->
            val value = array.getString(index)
                ?: throw IllegalArgumentException("launch.argv[$index] must be a string")
            require(value.isNotEmpty() && !value.contains('\u0000')) {
                "launch.argv[$index] must not be empty or contain a NUL byte"
            }
            value
        }
    }

    data class ProotProfile(
        val id: String,
        val name: String,
        val rootfs: File,
        val shell: String,
        val display: String,
        val launchArgv: List<String>?,
        val graphics: GraphicsProfile,
    )

    data class ChrootProfile(
        val id: String,
        val name: String,
        val rootfs: String,
        val shell: String,
        val display: String,
        val launchArgv: List<String>?,
        val graphics: GraphicsProfile,
    )

    data class DroidspacesProfile(
        val id: String,
        val name: String,
        val container: String,
        val user: String,
        val display: String,
        val launchArgv: List<String>?,
        val graphics: GraphicsProfile,
    )

    data class GraphicsProfile(
        val renderer: String,
        val qtQuickBackend: String,
    ) {
        fun environment(): List<String> = buildList {
            if (renderer == GRAPHICS_LLVMPIPE) {
                add("LIBGL_ALWAYS_SOFTWARE=1")
                add("GALLIUM_DRIVER=llvmpipe")
                add("MESA_LOADER_DRIVER_OVERRIDE=llvmpipe")
            }
            if (renderer == GRAPHICS_VIRGL) {
                add("LIBGL_ALWAYS_SOFTWARE=0")
                add("GALLIUM_DRIVER=virpipe")
                add("MESA_LOADER_DRIVER_OVERRIDE=virpipe")
            }
            if (qtQuickBackend == QT_QUICK_SOFTWARE) add("QT_QUICK_BACKEND=software")
        }
    }

    companion object {
        private const val EXTENSION = "toml"
        private val ID_PATTERN = Regex("[a-z0-9]+(?:-[a-z0-9]+)*")
        const val RUNTIME_INTERNAL_SHELL = "internal-shell"
        const val RUNTIME_PROOT = "proot"
        const val RUNTIME_CHROOT = "chroot"
        const val RUNTIME_DROIDSPACES = "droidspaces"
        const val DISPLAY_NONE = "none"
        const val DISPLAY_X11 = "x11"
        const val DISPLAY_WAYLAND = "wayland"
        const val GRAPHICS_AUTO = "auto"
        const val GRAPHICS_LLVMPIPE = "llvmpipe"
        const val GRAPHICS_VIRGL = "virgl"
        const val QT_QUICK_SOFTWARE = "software"
        const val EXTRA_RUNTIME = "app.trierarch.config.RUNTIME"
    }
}
