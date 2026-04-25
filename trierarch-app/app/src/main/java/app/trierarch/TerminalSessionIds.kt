package app.trierarch

/**
 * JNI 里 PTY 用单个 Int 做主键；逻辑上按二维管理：**[namespace][slot]**
 * - [0][x] Arch，[1][x] Wine，[2][x] Debian（与产品侧栏分页一致）
 * - 每个 namespace 的 **slot 0** 预留给该环境的「桌面 / 无头启动注入」；**1+** 为交互终端
 *
 * 编码：`nativeId = namespace * SESSION_STRIDE + slot`
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

    /** 与 `jni_context::RootfsKind` 一致：Arch=0, Debian=1, Wine=2 */
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

    /** 下拉/解析用：展示名 + 稳定可解析的 native id */
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
     * 在指定 [namespace] 里分配下一个交互 slot（当前「新建会话」仅向 Arch 命名空间追加）。
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
