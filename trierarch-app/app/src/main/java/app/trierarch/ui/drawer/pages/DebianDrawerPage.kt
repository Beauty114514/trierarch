package app.trierarch.ui.drawer.pages

import android.content.SharedPreferences
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DrawerState
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.unit.dp
import app.trierarch.R
import app.trierarch.TerminalSessionIds
import app.trierarch.ui.drawer.menu.DrawerExpandableSection
import app.trierarch.ui.drawer.menu.DrawerDropdownField
import app.trierarch.ui.drawer.menu.DrawerScriptEditor
import app.trierarch.ui.prefs.AppPrefs
import app.trierarch.ui.runtime.TerminalSessionController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

@Composable
fun DebianDrawerPage(
    prefs: SharedPreferences,
    drawerState: DrawerState,
    scope: CoroutineScope,
    terminalSessionState: TerminalSessionController.State,
    onTerminalSessionStateChange: (TerminalSessionController.State) -> Unit,
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
    x11ScriptEditorOpen: Boolean,
    onX11ScriptEditorOpenChange: (Boolean) -> Unit,
    onEnterDebianDesktop: () -> Unit,
    onEnterTerminal: () -> Unit,
    onExitDisplayModes: () -> Unit,
) {
    Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
        Text("Debian", color = Color.White.copy(alpha = 0.9f), style = MaterialTheme.typography.titleLarge)
        Spacer(Modifier.height(12.dp))
        Text(
            text = "X11",
            color = MaterialTheme.colorScheme.primary,
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier
                .fillMaxWidth()
                .pointerInput(Unit) {
                    detectTapGestures(
                        onLongPress = { onX11ScriptEditorOpenChange(true) },
                        onTap = {
                            scope.launch {
                                drawerState.close()
                                onEnterDebianDesktop()
                            }
                        },
                    )
                }
                .padding(vertical = 12.dp),
        )
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
                    .padding(horizontal = 12.dp, vertical = 8.dp),
            )
            Text(
                text = "Apply custom resolution",
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onX11ResolutionCustomApply() }
                    .padding(vertical = 8.dp, horizontal = 12.dp),
            )
        }
        DrawerExpandableSection(title = "Scripts", defaultExpanded = false) {
            if (x11ScriptEditorOpen) {
                DrawerScriptEditor(
                    title = "X11 startup script",
                    initialText = AppPrefs.readDebianDesktopStartupScript(prefs),
                    onSave = {
                        AppPrefs.writeDebianDesktopStartupScript(prefs, it)
                        onX11ScriptEditorOpenChange(false)
                    },
                )
            } else {
                Text(
                    text = "Edit X11 startup script",
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { onX11ScriptEditorOpenChange(true) }
                        .padding(vertical = 12.dp, horizontal = 12.dp),
                )
            }
        }
        Text(
            text = "Terminal",
            color = MaterialTheme.colorScheme.primary,
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier
                .fillMaxWidth()
                .clickable {
                    scope.launch { drawerState.close() }
                    onTerminalSessionStateChange(terminalSessionState.copy(activeSessionId = TerminalSessionIds.DEBIAN_TERMINAL))
                    onExitDisplayModes()
                    onEnterTerminal()
                }
                .padding(vertical = 12.dp)
        )
        Spacer(Modifier.height(520.dp))
    }
}

