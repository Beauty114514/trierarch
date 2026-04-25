package app.trierarch

/**
 * PTY sessions are keyed by a single `Int` in JNI; conceptually this is a **(namespace, slot)** grid:
 * - `[0][*]` Arch, `[1][*]` Wine, `[2][*]` Debian (same order as the in-app drawer tabs)
 * - **slot 0** in each namespace is reserved for that environment’s headless “desktop / inject” session;
 *   **slot 1+** are interactive terminals
 *
 * Encoding: `nativeId = namespace * SESSION_STRIDE + slot`
 */
object TerminalSessionIds {
    const val SESSION_STRIDE = 64

    const val NS_ARCH = 0
    const val NS_WINE = 1
    const val NS_DEBIAN = 2

    const val SLOT_HEADLESS_DISPLAY = 0
    const val SLOT_FIRST_INTERACTIVE = 1
    const val SLOT_LEGACY_ARCH_X11 = 2

    fun nativeId(namespace: Int, slot: Int): Int = namespace * SESSION_STRIDE + slot

    fun namespaceOf(nativeSessionId: Int): Int = nativeSessionId / SESSION_STRIDE
    fun slotOf(nativeSessionId: Int): Int = nativeSessionId % SESSION_STRIDE

    /** Matches `jni_context::RootfsKind`: Arch=0, Debian=1, Wine=2 */
    fun rootfsKindForNativeId(nativeSessionId: Int): Int = when (namespaceOf(nativeSessionId)) {
        NS_ARCH -> 0
        NS_DEBIAN -> 1
        NS_WINE -> 2
        else -> 0
    }

    fun terminalTabLabel(nativeSessionId: Int): String {
        val ns = namespaceOf(nativeSessionId)
        val slot = slotOf(nativeSessionId)
        val name = when (ns) {
            NS_ARCH -> "Arch"
            NS_WINE -> "Wine"
            NS_DEBIAN -> "Debian"
            else -> "?"
        }
        return "$name $slot"
    }

    /** Picker / round-trip line: display label plus a stable, parseable native id */
    fun sessionPickerLine(nativeSessionId: Int): String =
        "${terminalTabLabel(nativeSessionId)} · $nativeSessionId"

    fun parseSessionPickerLine(line: String): Int? =
        line.substringAfterLast('·', "").trim().toIntOrNull()

    val ARCH_WAYLAND_DISPLAY: Int = nativeId(NS_ARCH, SLOT_HEADLESS_DISPLAY)
    val WINE_HEADLESS_DISPLAY: Int = nativeId(NS_WINE, SLOT_HEADLESS_DISPLAY)
    val DEBIAN_X11_DISPLAY: Int = nativeId(NS_DEBIAN, SLOT_HEADLESS_DISPLAY)

    val ARCH_TERMINAL: Int = nativeId(NS_ARCH, SLOT_FIRST_INTERACTIVE)
    val WINE_TERMINAL: Int = nativeId(NS_WINE, SLOT_FIRST_INTERACTIVE)
    val DEBIAN_TERMINAL: Int = nativeId(NS_DEBIAN, SLOT_FIRST_INTERACTIVE)

    val LEGACY_ARCH_X11_PTY: Int = nativeId(NS_ARCH, SLOT_LEGACY_ARCH_X11)

    val FIRST_TERMINAL: Int = ARCH_TERMINAL

    /**
     * Picks the next free interactive slot in [namespace]. New-session only appends in Arch for now.
     */
    fun nextInteractiveNativeId(existing: List<Int>, namespace: Int): Int {
        val used = existing
            .filter { namespaceOf(it) == namespace && slotOf(it) >= SLOT_FIRST_INTERACTIVE }
            .map { slotOf(it) }
            .toSet()
        var s = SLOT_FIRST_INTERACTIVE
        while (s in used) s++
        require(s < SESSION_STRIDE) { "too many PTY sessions in namespace $namespace" }
        return nativeId(namespace, s)
    }
}
