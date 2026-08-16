package app.trierarch.runtime

import java.io.File

/** The built-in, zero-configuration runtime. */
object InternalShellLaunchSpec {
    fun create(filesDirectory: File, cacheDirectory: File): TerminalLaunchSpec {
        check(filesDirectory.isDirectory || filesDirectory.mkdirs()) {
            "Unable to create the internal workspace"
        }
        check(cacheDirectory.isDirectory || cacheDirectory.mkdirs()) {
            "Unable to create the internal shell cache directory"
        }
        val shellInitializationFile = File(cacheDirectory, "trierarch-mkshrc")
        shellInitializationFile.writeText(
            """
            . /system/etc/mkshrc
            TRIERARCH_ROOT="${'$'}PWD"
            trierarch_prompt_path() {
                case "${'$'}PWD" in
                    "${'$'}TRIERARCH_ROOT") REPLY='~' ;;
                    "${'$'}TRIERARCH_ROOT"/*) REPLY="~${'$'}{PWD#"${'$'}TRIERARCH_ROOT"}" ;;
                    *) REPLY="${'$'}PWD" ;;
                esac
            }
            PS1='${'$'}{| trierarch_prompt_path; }${'$'} '
            """.trimIndent() + "\n",
        )
        check(shellInitializationFile.isFile) {
            "Unable to create the internal shell initialization file"
        }

        return TerminalLaunchSpec(
            command = "/system/bin/sh",
            arguments = arrayOf("-i"),
            workingDirectory = filesDirectory,
            environment = arrayOf(
                "HOME=${filesDirectory.absolutePath}",
                "TMPDIR=${cacheDirectory.absolutePath}",
                "PATH=/system/bin:/system/xbin",
                "TERM=xterm-256color",
                "LANG=C.UTF-8",
                "ENV=${shellInitializationFile.absolutePath}",
            ),
        )
    }
}
