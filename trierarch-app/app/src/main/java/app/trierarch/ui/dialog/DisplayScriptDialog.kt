package app.trierarch.ui.dialog

import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import app.trierarch.ui.glass.GlassDialogWidthStandardDp
import app.trierarch.ui.glass.GlassOverlayLayer
import app.trierarch.ui.glass.GlassVerticalScrollbar
import app.trierarch.ui.glass.OrbStyleGlassPanel

private val ScriptEditorScrollMaxDp = 260.dp
private val ScriptEditorViewportForScrollbarDp = 236.dp

/**
 * Display startup script editor: same width cap and wrap-height shell as other glass sheets; script scrolls inside a bounded band.
 * Confirm: [onConfirm] with edited text; dismiss without commit: [onDismiss].
 */
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
    val scriptScroll = rememberScrollState()

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
                    "Display startup script",
                    style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                    color = Color.White
                )
                Box(Modifier.fillMaxWidth().padding(top = 10.dp)) {
                    SelectionContainer(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(end = 10.dp)
                            .heightIn(max = ScriptEditorScrollMaxDp)
                            .verticalScroll(scriptScroll)
                    ) {
                        OutlinedTextField(
                            value = script,
                            onValueChange = { script = it },
                            modifier = Modifier.fillMaxWidth(),
                            minLines = 4,
                            singleLine = false
                        )
                    }
                    GlassVerticalScrollbar(
                        scrollState = scriptScroll,
                        viewportHeight = ScriptEditorViewportForScrollbarDp,
                        modifier = Modifier
                            .align(Alignment.CenterEnd)
                            .padding(end = 2.dp)
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
