package app.trierarch.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.selection.LocalTextSelectionColors
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.text.selection.TextSelectionColors
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.isCtrlPressed
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.trierarch.NativeBridge
import app.trierarch.ui.AppStrings

@Composable
fun TerminalScreen(
    lines: List<String>,
    partialLine: String,
    inputLine: String,
    onInputChange: (String) -> Unit,
    onInputSubmit: (String) -> Unit
) {
    val focusRequester = remember { FocusRequester() }
    val listState = rememberLazyListState()
    val fgColor = MaterialTheme.colorScheme.onBackground
    val mono = FontFamily.Monospace

    LaunchedEffect(lines.size, partialLine) {
        val lastIndex = if (partialLine.isNotEmpty()) lines.size else (lines.size - 1).coerceAtLeast(0)
        listState.animateScrollToItem(lastIndex)
    }

    val promptDisplay = if (inputLine.isNotEmpty() && partialLine.endsWith(inputLine)) {
        partialLine.dropLast(inputLine.length)
    } else {
        partialLine
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .padding(6.dp)
            .imePadding()
    ) {
        val lightBlue = Color(0xFFB3E5FC)
        val lightBlueSelection = remember {
            TextSelectionColors(
                handleColor = Color(0xFF64B5F6),
                backgroundColor = lightBlue.copy(alpha = 0.6f)
            )
        }
        CompositionLocalProvider(LocalTextSelectionColors provides lightBlueSelection) {
            SelectionContainer {
                LazyColumn(
                    state = listState,
                    modifier = Modifier
                        .weight(1f)
                        .clickable { try { focusRequester.requestFocus() } catch (_: IllegalStateException) { } },
                    verticalArrangement = Arrangement.spacedBy(0.dp),
                    contentPadding = PaddingValues(top = 29.dp, bottom = 24.dp)
                ) {
                    if (lines.isEmpty() && partialLine.isEmpty()) {
                        item {
                            Text(AppStrings.STARTING_ARCH, color = fgColor, fontSize = 14.sp, fontFamily = mono)
                        }
                    }
                    if (partialLine.isNotEmpty()) {
                        item {
                            Text("Welcome to Trierarch!", color = fgColor, fontSize = 14.sp, fontFamily = mono)
                        }
                        item { Spacer(modifier = Modifier.height(4.dp).fillMaxWidth()) }
                    }
                    items(lines.size) { i ->
                        Text(lines[i], color = fgColor, fontSize = 14.sp, fontFamily = mono)
                    }
                    item(key = "input_row") {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .heightIn(min = 18.dp, max = 22.dp)
                                .clickable { try { focusRequester.requestFocus() } catch (_: IllegalStateException) { } },
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(promptDisplay.trimStart(), color = fgColor, fontSize = 14.sp, fontFamily = mono)
                            BasicTextField(
                                value = inputLine,
                                onValueChange = onInputChange,
                                modifier = Modifier
                                    .weight(1f)
                                    .focusRequester(focusRequester)
                                    .onPreviewKeyEvent { event ->
                                        if (event.type == KeyEventType.KeyDown) {
                                            val result = when {
                                                event.key == Key.DirectionUp -> {
                                                    NativeBridge.writeInput(byteArrayOf(0x1b, 0x5b, 0x41))
                                                    true
                                                }
                                                event.key == Key.DirectionDown -> {
                                                    NativeBridge.writeInput(byteArrayOf(0x1b, 0x5b, 0x42))
                                                    true
                                                }
                                                event.key == Key.DirectionLeft -> {
                                                    NativeBridge.writeInput(byteArrayOf(0x1b, 0x5b, 0x44))
                                                    false
                                                }
                                                event.key == Key.DirectionRight -> {
                                                    NativeBridge.writeInput(byteArrayOf(0x1b, 0x5b, 0x43))
                                                    false
                                                }
                                                event.key == Key.Escape -> {
                                                    NativeBridge.writeInput(byteArrayOf(0x1b))
                                                    true
                                                }
                                                event.isCtrlPressed && event.key == Key.U -> {
                                                    NativeBridge.writeInput(byteArrayOf(0x15))
                                                    true
                                                }
                                                else -> null
                                            }
                                            result ?: false
                                        } else false
                                    },
                                textStyle = TextStyle(color = fgColor, fontSize = 14.sp, fontFamily = mono),
                                cursorBrush = SolidColor(fgColor),
                                singleLine = true,
                                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                                keyboardActions = KeyboardActions(onSend = { onInputSubmit(inputLine) }),
                                decorationBox = { inner -> Box(modifier = Modifier.fillMaxWidth()) { inner() } }
                            )
                        }
                    }
                }
            }
        }
    }
}
