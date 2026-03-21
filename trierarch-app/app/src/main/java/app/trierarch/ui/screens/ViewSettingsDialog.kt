package app.trierarch.ui.screens

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

const val MOUSE_MODE_TABLET = 0     /* 平板：触摸=光标位置，长按右键 */
const val MOUSE_MODE_TOUCHPAD = 1   /* 触摸板：相对移动，整块=左键 */

private val PERCENT_OPTIONS = (10..100 step 10).toList()

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

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("View settings") },
        text = {
            Column(modifier = modifier.padding(vertical = 8.dp)) {
                Text("Mouse mode", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(bottom = 4.dp))
                Box(modifier = Modifier.padding(bottom = 12.dp)) {
                    TextButton(onClick = { mouseExpanded = true }) {
                        Text(if (mouseMode == MOUSE_MODE_TABLET) "Tablet" else "Touchpad")
                    }
                    DropdownMenu(
                        expanded = mouseExpanded,
                        onDismissRequest = { mouseExpanded = false }
                    ) {
                        DropdownMenuItem(
                            text = { Text("Touchpad") },
                            onClick = {
                                onMouseModeChange(MOUSE_MODE_TOUCHPAD)
                                mouseExpanded = false
                            }
                        )
                        DropdownMenuItem(
                            text = { Text("Tablet") },
                            onClick = {
                                onMouseModeChange(MOUSE_MODE_TABLET)
                                mouseExpanded = false
                            }
                        )
                    }
                }

                Text("Resolution", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(bottom = 4.dp))
                Box(modifier = Modifier.padding(bottom = 12.dp)) {
                    TextButton(onClick = { resolutionExpanded = true }) {
                        Text("${resolutionPercent.coerceIn(10, 100)}%")
                    }
                    DropdownMenu(
                        expanded = resolutionExpanded,
                        onDismissRequest = { resolutionExpanded = false }
                    ) {
                        PERCENT_OPTIONS.forEach { pct ->
                            DropdownMenuItem(
                                text = { Text("$pct%") },
                                onClick = {
                                    onResolutionPercentChange(pct)
                                    resolutionExpanded = false
                                }
                            )
                        }
                    }
                }

                Text("Scale", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(bottom = 4.dp))
                Box(modifier = Modifier.padding(bottom = 12.dp)) {
                    TextButton(onClick = { scaleExpanded = true }) {
                        Text("${scalePercent.coerceIn(10, 100)}%")
                    }
                    DropdownMenu(
                        expanded = scaleExpanded,
                        onDismissRequest = { scaleExpanded = false }
                    ) {
                        PERCENT_OPTIONS.forEach { pct ->
                            DropdownMenuItem(
                                text = { Text("$pct%") },
                                onClick = {
                                    onScalePercentChange(pct)
                                    scaleExpanded = false
                                }
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Done")
            }
        }
    )
}
