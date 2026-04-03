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
import app.trierarch.shell.ShellFonts
import app.trierarch.ui.glass.GlassDialogWidthPickerDp
import app.trierarch.ui.glass.GlassDialogWidthStandardDp
import app.trierarch.ui.glass.GlassPickerPanelMinHeightDp
import app.trierarch.ui.glass.GlassOverlayLayer
import app.trierarch.ui.glass.GlassSubOverlay
import app.trierarch.ui.glass.OrbStyleGlassPanel

private val TitleColor = Color.White

@Composable
private fun accent(): Color = MaterialTheme.colorScheme.primary

/**
 * Terminal font appearance: [GlassOverlayLayer]; font list uses [GlassSubOverlay].
 * Dismiss: [onDismiss] from scrim or system back.
 */
@Composable
fun AppearanceDialog(
    terminalFontPrefKey: String,
    onTerminalFontPrefChange: (String) -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    var fontPickerOpen by remember { mutableStateOf(false) }
    var lastMainCardHeight by remember { mutableStateOf<Dp?>(null) }
    var subPanelLock by remember { mutableStateOf<Dp?>(null) }
    val fontIndex = ShellFonts.indexForPref(terminalFontPrefKey)
    val fontLabel = ShellFonts.options[fontIndex].label
    val link = accent()

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
                    "Appearance",
                    style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                    color = TitleColor,
                    modifier = Modifier.padding(bottom = 8.dp)
                )
                Text(
                    "Font",
                    style = MaterialTheme.typography.titleMedium,
                    color = TitleColor,
                    modifier = Modifier.padding(bottom = 4.dp)
                )
                TextButton(
                    onClick = {
                        subPanelLock = lastMainCardHeight ?: GlassPickerPanelMinHeightDp
                        fontPickerOpen = true
                    },
                    modifier = Modifier.padding(bottom = 12.dp)
                ) {
                    Text(fontLabel, color = link)
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End
                ) {
                    TextButton(onClick = onDismiss) {
                        Text("Done", color = link)
                    }
                }
            }
        }
        if (fontPickerOpen) {
            GlassSubOverlay(onDismissRequest = {
                fontPickerOpen = false
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
                    ShellFonts.options.forEachIndexed { index, opt ->
                        val selected = index == fontIndex
                        TextButton(
                            onClick = {
                                onTerminalFontPrefChange(opt.id)
                                fontPickerOpen = false
                                subPanelLock = null
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                opt.label,
                                color = if (selected) link else TitleColor,
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
