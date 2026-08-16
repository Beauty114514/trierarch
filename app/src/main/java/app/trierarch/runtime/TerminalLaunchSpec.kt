package app.trierarch.runtime

import java.io.File

/** A concrete command executed by Trierarch's native PTY host. */
data class TerminalLaunchSpec(
    val command: String,
    val arguments: Array<String>,
    val workingDirectory: File,
    /** Complete `NAME=VALUE` process environment. */
    val environment: Array<String>,
)
