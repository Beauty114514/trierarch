package app.trierarch.runtime

import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class InternalShellLaunchSpecTest {
    @Test
    fun internalShellStartsInTheAppFilesDirectory() {
        val root = File(System.getProperty("java.io.tmpdir"), "trierarch-files")
        val cache = File(System.getProperty("java.io.tmpdir"), "trierarch-cache")
        root.deleteRecursively()
        cache.deleteRecursively()

        val spec = InternalShellLaunchSpec.create(root, cache)

        assertEquals("/system/bin/sh", spec.command)
        assertEquals(root, spec.workingDirectory)
        assertTrue(spec.arguments.contentEquals(arrayOf("-i")))
        assertTrue(spec.environment.contains("HOME=${root.absolutePath}"))
        assertTrue(spec.environment.contains("TERM=xterm-256color"))
    }
}
