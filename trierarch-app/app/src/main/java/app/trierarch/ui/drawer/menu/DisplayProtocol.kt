package app.trierarch.ui.drawer.menu

enum class DisplayProtocol(val label: String) {
    WAYLAND("Wayland"),
    X11("X11"),
    ;

    companion object {
        fun fromLabel(label: String): DisplayProtocol =
            entries.find { it.label == label } ?: WAYLAND
    }
}
