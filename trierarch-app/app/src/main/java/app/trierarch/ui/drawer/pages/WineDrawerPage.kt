package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DrawerState
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import app.trierarch.TerminalSessionIds
import app.trierarch.ui.runtime.TerminalSessionController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

@Composable
fun WineDrawerPage(
    drawerState: DrawerState,
    scope: CoroutineScope,
    terminalSessionState: TerminalSessionController.State,
    onTerminalSessionStateChange: (TerminalSessionController.State) -> Unit,
    onEnterTerminal: () -> Unit,
    onExitDisplayModes: () -> Unit,
) {
    Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
        Text("Wine", color = Color.White.copy(alpha = 0.9f), style = MaterialTheme.typography.titleLarge)
        Spacer(Modifier.height(12.dp))
        Text(
            text = "Terminal",
            color = MaterialTheme.colorScheme.primary,
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier
                .fillMaxWidth()
                .clickable {
                    scope.launch { drawerState.close() }
                    onTerminalSessionStateChange(terminalSessionState.copy(activeSessionId = TerminalSessionIds.WINE_TERMINAL))
                    onExitDisplayModes()
                    onEnterTerminal()
                }
                .padding(vertical = 12.dp)
        )
        Spacer(Modifier.height(520.dp))
    }
}

