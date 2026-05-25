package app.trierarch.ui.drawer.menu

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Code
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import app.trierarch.ui.drawer.pages.drawerPageAccent

@Composable
fun DrawerProtocolRow(
    protocol: DisplayProtocol,
    onProtocolSelect: (DisplayProtocol) -> Unit,
    onConfigureScript: () -> Unit,
    modifier: Modifier = Modifier,
    label: String = "Protocol",
) {
    val accent = drawerPageAccent()
    Row(
        modifier = modifier
            .fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        DrawerDropdownField(
            label = label,
            value = protocol.label,
            options = DisplayProtocol.entries.map { it.label },
            onSelect = { onProtocolSelect(DisplayProtocol.fromLabel(it)) },
            modifier = Modifier.weight(1f),
        )
        Spacer(Modifier.width(4.dp))
        IconButton(onClick = onConfigureScript) {
            Icon(
                imageVector = Icons.Filled.Code,
                contentDescription = "Edit startup script",
                tint = accent,
            )
        }
    }
}
