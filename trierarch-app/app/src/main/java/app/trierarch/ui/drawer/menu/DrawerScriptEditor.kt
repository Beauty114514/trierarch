package app.trierarch.ui.drawer.menu

import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
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
import androidx.compose.ui.unit.dp
import app.trierarch.ui.drawer.pages.drawerPageAccent

@Composable
fun DrawerScriptEditor(
    title: String,
    initialText: String,
    onSave: (String) -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var text by remember { mutableStateOf(initialText) }
    LaunchedEffect(initialText) { text = initialText }
    val accent = drawerPageAccent()
    val level = LocalDrawerHierarchyLevel.current

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .animateContentSize(),
    ) {
        Row(
            modifier = Modifier.padding(
                start = drawerStartPadding(level),
                end = 4.dp,
                top = drawerSectionVerticalPadding(level),
                bottom = drawerSectionVerticalPadding(level),
            ),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = title,
                style = drawerSectionTextStyle(level),
                color = accent,
                modifier = Modifier.weight(1f),
            )
            IconButton(onClick = onCancel) {
                Icon(
                    imageVector = Icons.Filled.ArrowBack,
                    contentDescription = "Back to protocol settings",
                    tint = accent,
                )
            }
        }
        OutlinedTextField(
            value = text,
            onValueChange = { text = it },
            minLines = 5,
            singleLine = false,
            modifier = modifier
                .fillMaxWidth()
                .padding(
                    start = drawerStartPadding(level),
                    end = drawerEndPadding(),
                ),
        )
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(
                    start = drawerStartPadding(level),
                    end = drawerEndPadding(),
                    top = 6.dp,
                    bottom = 6.dp,
                ),
            horizontalArrangement = Arrangement.End,
        ) {
            TextButton(onClick = { text = initialText }) {
                Text("Reset", color = accent.copy(alpha = 0.85f))
            }
            TextButton(onClick = { onSave(text.trimEnd()) }) {
                Text("Save", color = accent)
            }
        }
    }
}

