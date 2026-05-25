package app.trierarch.ui.drawer.menu

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

/** View settings for the in-app Wayland compositor (Arch nested desktop). */
@Composable
fun DrawerWaylandViewSettings(
    mouseModeLabel: String,
    onMouseModeSelectLabel: (String) -> Unit,
    resolutionPercentLabel: String,
    onResolutionPercentSelectLabel: (String) -> Unit,
    scalePercentLabel: String,
    onScalePercentSelectLabel: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    DrawerExpandableSection(title = "View", defaultExpanded = true, modifier = modifier) {
        DrawerDropdownField(
            label = "Mouse mode",
            value = mouseModeLabel,
            options = listOf("Touchpad", "Tablet"),
            onSelect = onMouseModeSelectLabel,
        )
        DrawerDropdownField(
            label = "Resolution",
            value = resolutionPercentLabel,
            options = (10..100 step 10).map { "${it}%" },
            onSelect = onResolutionPercentSelectLabel,
        )
        DrawerDropdownField(
            label = "Scale",
            value = scalePercentLabel,
            options = (100..1000 step 100).map { "${it}%" },
            onSelect = onScalePercentSelectLabel,
        )
    }
}
