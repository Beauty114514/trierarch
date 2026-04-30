package app.trierarch.ui.containers

internal enum class ContainerCreateTab(val title: String) {
    WINE_CONFIGURATION("Wine"),
    WIN_COMPONENTS("Win Components"),
    ENV_VARS("EnvVars"),
    DRIVES("Drives"),
    ADVANCED("Advanced"),
}

internal data class EnvVarRow(var key: String, var value: String)

internal data class DriveRow(var letter: String, var path: String)

internal data class PickerOption(val id: String, val label: String)

