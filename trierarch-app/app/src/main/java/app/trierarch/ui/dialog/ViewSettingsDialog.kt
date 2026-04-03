package app.trierarch.ui.dialog

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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import app.trierarch.ui.glass.GlassDialogWidthPickerDp
import app.trierarch.ui.glass.GlassDialogWidthStandardDp
import app.trierarch.ui.glass.GlassPickerPanelMinHeightDp
import app.trierarch.ui.glass.GlassOverlayLayer
import app.trierarch.ui.glass.GlassSubOverlay
import app.trierarch.ui.glass.OrbStyleGlassPanel

private val PERCENT_OPTIONS = (10..100 step 10).toList()
private val SCALE_OPTIONS = (100..1000 step 100).toList()
private val LabelColor = Color.White

@Composable
private fun linkColor(): Color = MaterialTheme.colorScheme.primary

/**
 * View/display routing settings: [GlassOverlayLayer] root; option lists in [GlassSubOverlay].
 * Dismiss: [onDismiss] from scrim or system back (and each sub-overlay’s back path).
 */
@Composable
fun ViewSettingsDialog(
    onDismiss: () -> Unit,
    mouseMode: Int,
    resolutionPercent: Int,
    scalePercent: Int,
    onMouseModeChange: (Int) -> Unit,
    onResolutionPercentChange: (Int) -> Unit,
    onScalePercentChange: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    var mousePickerOpen by remember { mutableStateOf(false) }
    var resolutionPickerOpen by remember { mutableStateOf(false) }
    var scalePickerOpen by remember { mutableStateOf(false) }
    var lastMainCardHeight by remember { mutableStateOf<Dp?>(null) }
    var subPanelLock by remember { mutableStateOf<Dp?>(null) }
    val accent = linkColor()

    GlassOverlayLayer(onDismissRequest = onDismiss) {
        val panelConsume = remember { MutableInteractionSource() }
        BoxWithConstraints(Modifier.fillMaxSize()) {
            OrbStyleGlassPanel(
                widthCap = GlassDialogWidthStandardDp,
                panelConsume = panelConsume,
                columnModifier = modifier,
                onCardHeightChanged = { h ->
                    if (lastMainCardHeight != h) lastMainCardHeight = h
                },
                contentPadding = PaddingValues(horizontal = 20.dp, vertical = 16.dp)
            ) {
                Text(
                    "View settings",
                    style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                    color = LabelColor,
                    modifier = Modifier.padding(bottom = 8.dp)
                )
                Text(
                    "Mouse mode",
                    style = MaterialTheme.typography.titleMedium,
                    color = LabelColor,
                    modifier = Modifier.padding(bottom = 4.dp)
                )
                TextButton(
                    onClick = { mousePickerOpen = true },
                    modifier = Modifier.padding(bottom = 12.dp)
                ) {
                    Text(
                        if (mouseMode == MOUSE_MODE_TABLET) "Tablet" else "Touchpad",
                        color = accent
                    )
                }
                Text(
                    "Resolution",
                    style = MaterialTheme.typography.titleMedium,
                    color = LabelColor,
                    modifier = Modifier.padding(bottom = 4.dp)
                )
                TextButton(
                    onClick = {
                        subPanelLock = lastMainCardHeight ?: GlassPickerPanelMinHeightDp
                        resolutionPickerOpen = true
                    },
                    modifier = Modifier.padding(bottom = 12.dp)
                ) {
                    Text(
                        "${resolutionPercent.coerceIn(10, 100)}%",
                        color = accent
                    )
                }
                Text(
                    "Scale",
                    style = MaterialTheme.typography.titleMedium,
                    color = LabelColor,
                    modifier = Modifier.padding(bottom = 4.dp)
                )
                TextButton(
                    onClick = {
                        subPanelLock = lastMainCardHeight ?: GlassPickerPanelMinHeightDp
                        scalePickerOpen = true
                    },
                    modifier = Modifier.padding(bottom = 12.dp)
                ) {
                    Text(
                        "${scalePercent.coerceIn(100, 1000)}%",
                        color = accent
                    )
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End
                ) {
                    TextButton(onClick = onDismiss) {
                        Text("Done", color = accent)
                    }
                }
            }
        }
        if (mousePickerOpen) {
            GlassSubOverlay(onDismissRequest = {
                mousePickerOpen = false
                subPanelLock = null
            }) {
                val pickConsume = remember { MutableInteractionSource() }
                OrbStyleGlassPanel(
                    widthCap = GlassDialogWidthPickerDp,
                    panelConsume = pickConsume,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                ) {
                    listOf("Touchpad", "Tablet").forEachIndexed { index, label ->
                        val selected = index == if (mouseMode == MOUSE_MODE_TOUCHPAD) 0 else 1
                        TextButton(
                            onClick = {
                                onMouseModeChange(
                                    if (index == 0) MOUSE_MODE_TOUCHPAD else MOUSE_MODE_TABLET
                                )
                                mousePickerOpen = false
                                subPanelLock = null
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                label,
                                color = if (selected) accent else LabelColor,
                                style = MaterialTheme.typography.bodyLarge,
                                fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal
                            )
                        }
                    }
                }
            }
        }
        if (resolutionPickerOpen) {
            GlassSubOverlay(onDismissRequest = {
                resolutionPickerOpen = false
                subPanelLock = null
            }) {
                val pickConsume = remember { MutableInteractionSource() }
                OrbStyleGlassPanel(
                    widthCap = GlassDialogWidthPickerDp,
                    panelConsume = pickConsume,
                    cardHeight = subPanelLock ?: lastMainCardHeight ?: GlassPickerPanelMinHeightDp,
                    showVerticalScrollbar = true,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                ) {
                    PERCENT_OPTIONS.forEach { pct ->
                        val label = "$pct%"
                        val selected = pct == resolutionPercent.coerceIn(10, 100)
                        TextButton(
                            onClick = {
                                onResolutionPercentChange(pct)
                                resolutionPickerOpen = false
                                subPanelLock = null
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                label,
                                color = if (selected) accent else LabelColor,
                                style = MaterialTheme.typography.bodyLarge,
                                fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal
                            )
                        }
                    }
                }
            }
        }
        if (scalePickerOpen) {
            GlassSubOverlay(onDismissRequest = {
                scalePickerOpen = false
                subPanelLock = null
            }) {
                val pickConsume = remember { MutableInteractionSource() }
                OrbStyleGlassPanel(
                    widthCap = GlassDialogWidthPickerDp,
                    panelConsume = pickConsume,
                    cardHeight = subPanelLock ?: lastMainCardHeight ?: GlassPickerPanelMinHeightDp,
                    showVerticalScrollbar = true,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                ) {
                    SCALE_OPTIONS.forEach { pct ->
                        val label = "$pct%"
                        val selected = pct == scalePercent.coerceIn(100, 1000)
                        TextButton(
                            onClick = {
                                onScalePercentChange(pct)
                                scalePickerOpen = false
                                subPanelLock = null
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                label,
                                color = if (selected) accent else LabelColor,
                                style = MaterialTheme.typography.bodyLarge,
                                fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal
                            )
                        }
                    }
                }
            }
        }
    }
}
