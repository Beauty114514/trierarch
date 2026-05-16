package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.winlator.WinlatorHost
import app.trierarch.ui.drawer.menu.DrawerExpandableSection

@Composable
fun WineDrawerPage(
    onOpenContainers: () -> Unit,
    winlatorHost: WinlatorHost? = null,
    onCloseDrawer: () -> Unit = {},
) {
    val accent = drawerPageAccent()

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp)
    ) {
        Text(
            "Wine",
            color = accent,
            style = MaterialTheme.typography.titleLarge
        )
        Spacer(Modifier.height(12.dp))

        if (winlatorHost == null) {
            Text(
                text = "Manage Wine containers and related assets.",
                color = accent.copy(alpha = 0.72f),
                style = MaterialTheme.typography.bodyMedium,
            )
            Spacer(Modifier.height(16.dp))
            Text(
                text = "Container",
                style = MaterialTheme.typography.bodyLarge,
                color = accent,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onOpenContainers() }
                    .padding(vertical = 12.dp)
            )
        } else {
            DrawerExpandableSection(title = "Controls", defaultExpanded = true) {
                DrawerTextItem("Keyboard") { winlatorHost.showKeyboard(); onCloseDrawer() }
                DrawerTextItem("Input Controls") { winlatorHost.showInputControlsDialog(); onCloseDrawer() }
                DrawerTextItem("Toggle Fullscreen") { winlatorHost.toggleFullscreen(); onCloseDrawer() }
                DrawerTextItem("Task Manager") { winlatorHost.showTaskManagerDialog(); onCloseDrawer() }
                DrawerTextItem("Active Windows") { winlatorHost.showActiveWindowsDialog(); onCloseDrawer() }
                DrawerTextItem("Magnifier") { winlatorHost.toggleMagnifier(); onCloseDrawer() }
                DrawerTextItem("Screen Effect") { winlatorHost.showScreenEffectDialog(); onCloseDrawer() }
                DrawerTextItem("Logs") { winlatorHost.showDebugDialog(); onCloseDrawer() }
                DrawerTextItem("Touchpad Help") { winlatorHost.showTouchpadHelpDialog(); onCloseDrawer() }
            }

            Spacer(Modifier.height(8.dp))

            Text(
                text = "Exit",
                style = MaterialTheme.typography.bodyLarge,
                color = Color(0xFFB00020),
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { winlatorHost.exit(); onCloseDrawer() }
                    .padding(vertical = 12.dp)
            )

            Spacer(Modifier.height(16.dp))

            Text(
                text = "Switch Container",
                style = MaterialTheme.typography.bodyLarge,
                color = accent,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onOpenContainers() }
                    .padding(vertical = 12.dp)
            )
        }
    }
}

@Composable
private fun DrawerTextItem(
    title: String,
    onClick: () -> Unit,
) {
    val accent = drawerPageAccent()
    Text(
        text = title,
        style = MaterialTheme.typography.bodyLarge,
        color = accent,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 12.dp, horizontal = 12.dp)
    )
}

