package app.trierarch.ui.dialog

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.window.DialogWindowProvider
import app.trierarch.shell.ShellFonts
import app.trierarch.ui.glass.FloatingGlassCornerDp
import app.trierarch.ui.glass.FloatingGlassRimAlpha
import app.trierarch.ui.glass.FloatingGlassRimDp
import app.trierarch.ui.glass.floatingGlassBrush
import app.trierarch.ui.glass.floatingOverlayScrimColor
import app.trierarch.ui.glass.glassBlurModifier
import app.trierarch.ui.glass.glassPanelEnter
import app.trierarch.ui.glass.glassPanelExit
import app.trierarch.ui.glass.glassScrimEnter
import app.trierarch.ui.glass.glassScrimExit

private val TitleColor = Color.White

@Composable
private fun accent(): Color = MaterialTheme.colorScheme.primary

@Composable
fun AppearanceDialog(
    terminalFontPrefKey: String,
    onTerminalFontPrefChange: (String) -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    var fontPickerOpen by remember { mutableStateOf(false) }
    val fontIndex = ShellFonts.indexForPref(terminalFontPrefKey)
    val fontLabel = ShellFonts.options[fontIndex].label

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        val view = LocalView.current
        SideEffect {
            (view.parent as? DialogWindowProvider)?.window?.setDimAmount(0f)
        }
        val scrimSource = remember { MutableInteractionSource() }
        val panelConsume = remember { MutableInteractionSource() }
        val shape = RoundedCornerShape(FloatingGlassCornerDp)
        val link = accent()
        val overlayShown = remember { MutableTransitionState(false) }
        LaunchedEffect(Unit) { overlayShown.targetState = true }

        Box(modifier = Modifier.fillMaxSize()) {
            AnimatedVisibility(
                visibleState = overlayShown,
                enter = glassScrimEnter(),
                exit = glassScrimExit()
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(floatingOverlayScrimColor())
                        .clickable(
                            interactionSource = scrimSource,
                            indication = null,
                            onClick = onDismiss
                        )
                )
            }
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                AnimatedVisibility(
                    visibleState = overlayShown,
                    enter = glassPanelEnter(),
                    exit = glassPanelExit()
                ) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth(0.9f)
                            .widthIn(max = 400.dp)
                            .height(IntrinsicSize.Min)
                            .clip(shape)
                            .border(
                                width = FloatingGlassRimDp,
                                color = MaterialTheme.colorScheme.outline.copy(alpha = FloatingGlassRimAlpha),
                                shape = shape
                            )
                            .clickable(
                                interactionSource = panelConsume,
                                indication = null,
                                onClick = { }
                            )
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .fillMaxHeight()
                                .then(glassBlurModifier())
                                .background(brush = floatingGlassBrush(), shape = shape)
                        )
                        Column(
                            modifier = modifier
                                .fillMaxWidth()
                                .padding(horizontal = 20.dp, vertical = 16.dp)
                        ) {
                            Text(
                                "Appearance",
                                style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                                color = TitleColor,
                                modifier = Modifier.padding(bottom = 8.dp)
                            )
                            Text(
                                "Font",
                                style = MaterialTheme.typography.titleSmall,
                                color = TitleColor,
                                modifier = Modifier.padding(bottom = 4.dp)
                            )
                            TextButton(
                                onClick = { fontPickerOpen = true },
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
                            if (fontPickerOpen) {
                                SingleChoicePicker(
                                    options = ShellFonts.options.map { it.label },
                                    selectedIndex = fontIndex,
                                    onSelect = { idx ->
                                        onTerminalFontPrefChange(ShellFonts.options[idx].id)
                                    },
                                    onDismiss = { fontPickerOpen = false }
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}
