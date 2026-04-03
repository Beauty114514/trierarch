package app.trierarch.ui.dialog

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import app.trierarch.ui.glass.GlassDialogWidthStandardDp
import app.trierarch.ui.glass.GlassOverlayLayer
import app.trierarch.ui.glass.OrbStyleGlassPanel

private val LabelColor = Color.White

/**
 * Terminal session picker: [GlassOverlayLayer] and [OrbStyleGlassPanel] only (no nested sub-overlay).
 * Dismiss: [onDismiss] from scrim or system back.
 */
@Composable
fun SessionsDialog(
    sessionIds: List<Int>,
    activeSessionId: Int,
    onSelectSession: (Int) -> Unit,
    onAddSession: () -> Unit,
    onCloseSession: (Int) -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    val accent = MaterialTheme.colorScheme.primary
    GlassOverlayLayer(onDismissRequest = onDismiss) {
        val panelConsume = remember { MutableInteractionSource() }
        BoxWithConstraints(Modifier.fillMaxSize()) {
            OrbStyleGlassPanel(
                widthCap = GlassDialogWidthStandardDp,
                panelConsume = panelConsume,
                columnModifier = modifier,
                contentPadding = PaddingValues(horizontal = 20.dp, vertical = 16.dp)
            ) {
                Text(
                    "Session",
                    style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                    color = LabelColor,
                    modifier = Modifier.padding(bottom = 12.dp)
                )
                sessionIds.sorted().forEach { id ->
                    val isActive = id == activeSessionId
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = 6.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = if (isActive) "Session $id (active)" else "Session $id",
                            color = if (isActive) accent else LabelColor,
                            style = MaterialTheme.typography.bodyLarge,
                            modifier = Modifier
                                .weight(1f)
                                .clickable { onSelectSession(id) }
                                .padding(vertical = 8.dp)
                        )
                        val canClose = sessionIds.size > 1
                        Text(
                            text = "−",
                            color = if (canClose) accent else accent.copy(alpha = 0.35f),
                            style = MaterialTheme.typography.titleLarge,
                            modifier = Modifier
                                .clickable(enabled = canClose) { onCloseSession(id) }
                                .padding(horizontal = 12.dp, vertical = 8.dp)
                        )
                    }
                }
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    TextButton(onClick = onAddSession) {
                        Text("+ New session", color = accent)
                    }
                    TextButton(onClick = onDismiss) {
                        Text("Done", color = accent)
                    }
                }
            }
        }
    }
}
