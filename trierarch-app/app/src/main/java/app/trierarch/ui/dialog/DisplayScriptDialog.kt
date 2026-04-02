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
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
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

@Composable
fun DisplayScriptDialog(
    initialScript: String,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    var script by remember { mutableStateOf(initialScript) }
    LaunchedEffect(initialScript) {
        script = initialScript
    }

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
        LaunchedEffect(Unit) {
            overlayShown.targetState = true
        }

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
                            .fillMaxWidth(0.92f)
                            .widthIn(max = 520.dp)
                            .height(360.dp)
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
                                .fillMaxSize()
                                .then(glassBlurModifier())
                                .background(brush = floatingGlassBrush(), shape = shape)
                        )
                        Column(
                            modifier = modifier
                                .fillMaxSize()
                                .padding(horizontal = 20.dp, vertical = 16.dp)
                        ) {
                            Text(
                                "Display startup script",
                                style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                                color = MaterialTheme.colorScheme.onSurface
                            )
                            SelectionContainer(
                                modifier = Modifier
                                    .padding(top = 10.dp)
                                    .weight(1f)
                                    .verticalScroll(rememberScrollState())
                            ) {
                                OutlinedTextField(
                                    value = script,
                                    onValueChange = { script = it },
                                    modifier = Modifier.fillMaxWidth(),
                                    minLines = 6,
                                    maxLines = 14,
                                    singleLine = false
                                )
                            }
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(top = 8.dp),
                                horizontalArrangement = Arrangement.End
                            ) {
                                TextButton(onClick = onDismiss) {
                                    Text("Cancel")
                                }
                                TextButton(onClick = { onConfirm(script) }) {
                                    Text("Done")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
