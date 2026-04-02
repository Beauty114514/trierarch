package app.trierarch.ui.dialog

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
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
import androidx.compose.runtime.remember
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

private val PickerLabelColor = Color.White

@Composable
private fun pickerAccent(): Color = MaterialTheme.colorScheme.primary

/** Full-screen scrim + compact frosted column of options (shared by settings dialogs). */
@Composable
fun SingleChoicePicker(
    options: List<String>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit,
    onDismiss: () -> Unit
) {
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
            AnimatedVisibility(
                visibleState = overlayShown,
                enter = glassPanelEnter(),
                exit = glassPanelExit()
            ) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Box(
                        modifier = Modifier
                            .widthIn(min = 120.dp, max = 280.dp)
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
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 12.dp, vertical = 6.dp)
                        ) {
                            options.forEachIndexed { index, label ->
                                val isSelected = index == selectedIndex
                                TextButton(
                                    onClick = {
                                        onSelect(index)
                                        onDismiss()
                                    },
                                    modifier = Modifier.fillMaxWidth()
                                ) {
                                    Text(
                                        label,
                                        color = if (isSelected) pickerAccent() else PickerLabelColor,
                                        style = MaterialTheme.typography.bodyMedium,
                                        fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
