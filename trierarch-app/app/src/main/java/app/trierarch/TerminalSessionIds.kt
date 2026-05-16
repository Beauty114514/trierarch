package app.trierarch

/**
 * PTY sessions are keyed by a single [Int] in JNI. Layout is a **2×64 table**:
 *
 * ```
 *              col 0              col 1           col 2 …
 * row 0 Arch   Wayland headless   X11 headless    interactive terminals
 * row 1 Debian Wayland headless   X11 headless    interactive terminals
 * ```
 *
 * Encoding: `nativeId = rootfsRow × SLOTS_PER_ROOTFS + ptyCol`
 */
object TerminalSessionIds {
    const val SLOTS_PER_ROOTFS = 64

    /** Rootfs row in the PTY table (Arch = 0, Debian = 1). */
    enum class RootfsRow(val index: Int) {
        ARCH(0),
        DEBIAN(1),
    }

    /** Column within a rootfs row; interactive sessions use col ≥ [INTERACTIVE_FIRST]. */
    enum class PtyCol(val index: Int) {
        WAYLAND_HEADLESS(0),
        X11_HEADLESS(1),
        INTERACTIVE_FIRST(2),
    }

    fun nativeId(rootfs: RootfsRow, col: PtyCol): Int =
        rootfs.index * SLOTS_PER_ROOTFS + col.index

    fun nativeId(rootfsRow: Int, ptyCol: Int): Int =
        rootfsRow * SLOTS_PER_ROOTFS + ptyCol

    fun rootfsRowOf(nativeSessionId: Int): RootfsRow =
        when (nativeSessionId / SLOTS_PER_ROOTFS) {
            RootfsRow.ARCH.index -> RootfsRow.ARCH
            RootfsRow.DEBIAN.index -> RootfsRow.DEBIAN
            else -> RootfsRow.ARCH
        }

    fun ptyColOf(nativeSessionId: Int): Int = nativeSessionId % SLOTS_PER_ROOTFS

    /** @deprecated Use [rootfsRowOf]; kept for call sites passing raw row index. */
    fun namespaceOf(nativeSessionId: Int): Int = rootfsRowOf(nativeSessionId).index

    /** @deprecated Use [ptyColOf] */
    fun slotOf(nativeSessionId: Int): Int = ptyColOf(nativeSessionId)

    /** Matches `jni_context::RootfsKind`: Arch=0, Debian=1 */
    fun rootfsKindForNativeId(nativeSessionId: Int): Int = when (rootfsRowOf(nativeSessionId)) {
        RootfsRow.ARCH -> 0
        RootfsRow.DEBIAN -> 1
    }

    fun terminalTabLabel(nativeSessionId: Int): String {
        val rootfs = rootfsRowOf(nativeSessionId)
        val col = ptyColOf(nativeSessionId)
        val name = when (rootfs) {
            RootfsRow.ARCH -> "Arch"
            RootfsRow.DEBIAN -> "Debian"
        }
        return "$name $col"
    }

    fun sessionPickerLine(nativeSessionId: Int): String =
        "${terminalTabLabel(nativeSessionId)} · $nativeSessionId"

    fun parseSessionPickerLine(line: String): Int? =
        line.substringAfterLast('·', "").trim().toIntOrNull()

    val ARCH_WAYLAND_HEADLESS: Int = nativeId(RootfsRow.ARCH, PtyCol.WAYLAND_HEADLESS)
    val ARCH_X11_HEADLESS: Int = nativeId(RootfsRow.ARCH, PtyCol.X11_HEADLESS)
    val ARCH_TERMINAL: Int = nativeId(RootfsRow.ARCH, PtyCol.INTERACTIVE_FIRST)

    val DEBIAN_WAYLAND_HEADLESS: Int = nativeId(RootfsRow.DEBIAN, PtyCol.WAYLAND_HEADLESS)
    val DEBIAN_X11_HEADLESS: Int = nativeId(RootfsRow.DEBIAN, PtyCol.X11_HEADLESS)
    val DEBIAN_TERMINAL: Int = nativeId(RootfsRow.DEBIAN, PtyCol.INTERACTIVE_FIRST)

    val FIRST_TERMINAL: Int = ARCH_TERMINAL

    /** Next free interactive column (col ≥ 2) on [rootfsRow]. */
    fun nextInteractiveNativeId(existing: List<Int>, rootfsRow: Int): Int {
        val firstCol = PtyCol.INTERACTIVE_FIRST.index
        val used = existing
            .filter { namespaceOf(it) == rootfsRow && ptyColOf(it) >= firstCol }
            .map { ptyColOf(it) }
            .toSet()
        var col = firstCol
        while (col in used) col++
        require(col < SLOTS_PER_ROOTFS) { "too many PTY sessions on rootfs row $rootfsRow" }
        return nativeId(rootfsRow, col)
    }

    fun nextInteractiveNativeId(existing: List<Int>, rootfs: RootfsRow): Int =
        nextInteractiveNativeId(existing, rootfs.index)

    const val NS_ARCH: Int = 0
    const val NS_DEBIAN: Int = 1
}
