package app.trierarch.ui.drawer.menu

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringArrayResource
import app.trierarch.R
import app.trierarch.ui.drawer.pages.drawerPageAccent

/** View settings for [com.termux.x11.EmbeddedX11Controller] (Arch/Debian X11 surfaces). */
@Composable
fun DrawerEmbeddedX11ViewSettings(
    x11MouseModeLabel: String,
    onX11MouseModeSelectLabel: (String) -> Unit,
    x11ResolutionModeLabel: String,
    onX11ResolutionModeSelectLabel: (String) -> Unit,
    x11DisplayScaleLabel: String,
    onX11DisplayScaleSelectLabel: (String) -> Unit,
    x11ResolutionExactLabel: String,
    onX11ResolutionExactSelectLabel: (String) -> Unit,
    x11ResolutionCustom: String,
    onX11ResolutionCustomChange: (String) -> Unit,
    onX11ResolutionCustomApply: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val accent = drawerPageAccent()
    DrawerExpandableSection(title = "View", defaultExpanded = true, modifier = modifier) {
        DrawerDropdownField(
            label = "Mouse mode",
            value = x11MouseModeLabel,
            options = listOf("Touchpad", "Touch"),
            onSelect = onX11MouseModeSelectLabel,
        )
        DrawerDropdownField(
            label = "X11 resolution mode",
            value = x11ResolutionModeLabel,
            options = listOf("Native", "Scaled", "Fixed size", "Custom"),
            onSelect = onX11ResolutionModeSelectLabel,
        )
        if (x11ResolutionModeLabel == "Scaled") {
            DrawerDropdownField(
                label = "Display scale (%)",
                value = x11DisplayScaleLabel,
                options = (30..300 step 10).map { "$it%" },
                onSelect = onX11DisplayScaleSelectLabel,
            )
        }
        if (x11ResolutionModeLabel == "Fixed size") {
            val exactOptions = stringArrayResource(R.array.displayResolution).toList()
            DrawerDropdownField(
                label = "Fixed resolution",
                value = x11ResolutionExactLabel,
                options = exactOptions,
                onSelect = onX11ResolutionExactSelectLabel,
            )
        }
        if (x11ResolutionModeLabel == "Custom") {
            OutlinedTextField(
                value = x11ResolutionCustom,
                onValueChange = onX11ResolutionCustomChange,
                label = { Text("Custom WxH (e.g. 1920x1080)") },
                singleLine = true,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(
                        start = drawerStartPadding(),
                        end = drawerEndPadding(),
                        top = drawerRowVerticalPadding(),
                        bottom = drawerRowVerticalPadding(),
                    ),
            )
            Text(
                text = "Apply custom resolution",
                style = drawerRowTextStyle(),
                color = accent,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onX11ResolutionCustomApply() }
                    .padding(
                        start = drawerStartPadding(),
                        end = drawerEndPadding(),
                        top = drawerRowVerticalPadding(),
                        bottom = drawerRowVerticalPadding(),
                    ),
            )
        }
    }
}
