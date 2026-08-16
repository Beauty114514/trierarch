package app.trierarch.config

import android.content.Context
import org.tomlj.Toml
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
        )
    }

    /** Attaches to a container that DroidSpaces has already deployed and started. */
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
        )
    }

    private fun parse(text: String) = Toml.parse(text).also { parsed ->
        require(!parsed.hasErrors()) {
            parsed.errors().joinToString("\n") { error -> error.toString() }
        }
    }

    private fun required(value: String?, key: String): String = value?.trim().takeUnless { it.isNullOrEmpty() }
        ?: throw IllegalArgumentException("$key is required")

    data class ProotProfile(
        val id: String,
        val name: String,
        val rootfs: File,
        val shell: String,
    )

    data class ChrootProfile(
        val id: String,
        val name: String,
        val rootfs: String,
        val shell: String,
    )

    data class DroidspacesProfile(
        val id: String,
        val name: String,
        val container: String,
        val user: String,
    )

    companion object {
        private const val EXTENSION = "toml"
        private val ID_PATTERN = Regex("[a-z0-9]+(?:-[a-z0-9]+)*")
        const val RUNTIME_INTERNAL_SHELL = "internal-shell"
        const val RUNTIME_PROOT = "proot"
        const val RUNTIME_CHROOT = "chroot"
        const val RUNTIME_DROIDSPACES = "droidspaces"
        const val EXTRA_RUNTIME = "app.trierarch.config.RUNTIME"
    }
}
