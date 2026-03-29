package app.trierarch.ui.screens

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
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.window.DialogWindowProvider
import app.trierarch.ui.FloatingGlassCornerDp
import app.trierarch.ui.FloatingGlassRimAlpha
import app.trierarch.ui.FloatingGlassRimDp
import app.trierarch.ui.floatingGlassBrush
import app.trierarch.ui.floatingOverlayScrimColor
import app.trierarch.ui.glassBlurModifier
import app.trierarch.ui.glassPanelEnter
import app.trierarch.ui.glassPanelExit
import app.trierarch.ui.glassScrimEnter
import app.trierarch.ui.glassScrimExit

const val MOUSE_MODE_TABLET = 0 /* 平板：触摸=光标位置，长按右键 */
const val MOUSE_MODE_TOUCHPAD = 1 /* 触摸板：相对移动，整块=左键 */

private val PERCENT_OPTIONS = (10..100 step 10).toList()

private val LabelColor = Color.White

@Composable
private fun interactiveLabelColor(): Color = MaterialTheme.colorScheme.primary

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
    var mouseExpanded by remember { mutableStateOf(false) }
    var resolutionExpanded by remember { mutableStateOf(false) }
    var scaleExpanded by remember { mutableStateOf(false) }

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
        val linkColor = interactiveLabelColor()
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
                                onClick = { /* consume; inner controls handle actions */ }
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
                        "View settings",
                        style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                        color = LabelColor,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                    Text(
                        "Mouse mode",
                        style = MaterialTheme.typography.titleSmall,
                        color = LabelColor,
                        modifier = Modifier.padding(bottom = 4.dp)
                    )
                    Box(modifier = Modifier.padding(bottom = 12.dp)) {
                        TextButton(onClick = { mouseExpanded = true }) {
                            Text(
                                if (mouseMode == MOUSE_MODE_TABLET) "Tablet" else "Touchpad",
                                color = linkColor
                            )
                        }
                        DropdownMenu(
                            expanded = mouseExpanded,
                            onDismissRequest = { mouseExpanded = false }
                        ) {
                            DropdownMenuItem(
                                text = { Text("Touchpad", color = linkColor) },
                                onClick = {
                                    onMouseModeChange(MOUSE_MODE_TOUCHPAD)
                                    mouseExpanded = false
                                }
                            )
                            DropdownMenuItem(
                                text = { Text("Tablet", color = linkColor) },
                                onClick = {
                                    onMouseModeChange(MOUSE_MODE_TABLET)
                                    mouseExpanded = false
                                }
                            )
                        }
                    }

                    Text(
                        "Resolution",
                        style = MaterialTheme.typography.titleSmall,
                        color = LabelColor,
                        modifier = Modifier.padding(bottom = 4.dp)
                    )
                    Box(modifier = Modifier.padding(bottom = 12.dp)) {
                        TextButton(onClick = { resolutionExpanded = true }) {
                            Text(
                                "${resolutionPercent.coerceIn(10, 100)}%",
                                color = linkColor
                            )
                        }
                        DropdownMenu(
                            expanded = resolutionExpanded,
                            onDismissRequest = { resolutionExpanded = false }
                        ) {
                            PERCENT_OPTIONS.forEach { pct ->
                                DropdownMenuItem(
                                    text = { Text("$pct%", color = linkColor) },
                                    onClick = {
                                        onResolutionPercentChange(pct)
                                        resolutionExpanded = false
                                    }
                                )
                            }
                        }
                    }

                    Text(
                        "Scale",
                        style = MaterialTheme.typography.titleSmall,
                        color = LabelColor,
                        modifier = Modifier.padding(bottom = 4.dp)
                    )
                    Box(modifier = Modifier.padding(bottom = 12.dp)) {
                        TextButton(onClick = { scaleExpanded = true }) {
                            Text(
                                "${scalePercent.coerceIn(10, 100)}%",
                                color = linkColor
                            )
                        }
                        DropdownMenu(
                            expanded = scaleExpanded,
                            onDismissRequest = { scaleExpanded = false }
                        ) {
                            PERCENT_OPTIONS.forEach { pct ->
                                DropdownMenuItem(
                                    text = { Text("$pct%", color = linkColor) },
                                    onClick = {
                                        onScalePercentChange(pct)
                                        scaleExpanded = false
                                    }
                                )
                            }
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.End
                    ) {
                        TextButton(onClick = onDismiss) {
                            Text("Done", color = linkColor)
                        }
                    }
                }
                    }
                }
            }
        }
    }
}
