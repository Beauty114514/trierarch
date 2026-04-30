package app.trierarch.ui.containers

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier

/**
 * Mirrors upstream [com.winlator.ContainerDetailFragment.createWinComponentsTab] grouping
 * and [com.winlator.core.StringUtils.getString] labels (Winlator `values/strings.xml`, en).
 */
internal enum class WinComponentSection {
    DIRECTX,
    GENERAL,
}

internal fun winComponentSectionForKey(key: String): WinComponentSection =
    if (key.startsWith("direct") || key.startsWith("x")) {
        WinComponentSection.DIRECTX
    } else {
        WinComponentSection.GENERAL
    }

/** Human-readable row label; keys match [com.winlator.container.Container] wincomponent ids. */
internal fun winComponentLabelForKey(key: String): String =
    when (key) {
        "direct3d" -> "Direct3D"
        "directsound" -> "DirectSound"
        "directmusic" -> "DirectMusic"
        "directplay" -> "DirectPlay"
        "directshow" -> "DirectShow"
        "xaudio" -> "XAudio"
        "vcrun2005" -> "Visual C++ 2005"
        "vcrun2010" -> "Visual C++ 2010"
        "wmdecoder" -> "Windows Media Decoder"
        else -> key
    }

/** Matches Winlator `wincomponent_entries` (`builtin_wine` / `native_windows`). */
internal val WIN_COMPONENT_OPTION_LABELS = listOf("Builtin (Wine)", "Native (Windows)")

@Composable
internal fun WinComponentOptionRows(
    keys: List<String>,
    selectedIndex: (String) -> Int,
    onSelect: (String, Int) -> Unit,
) {
    keys.forEach { key ->
        var menuOpen by remember(key) { mutableStateOf(false) }
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(winComponentLabelForKey(key))
            Box {
                TextButton(onClick = { menuOpen = true }) {
                    Text(WIN_COMPONENT_OPTION_LABELS.getOrNull(selectedIndex(key)) ?: WIN_COMPONENT_OPTION_LABELS[0])
                }
                DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
                    WIN_COMPONENT_OPTION_LABELS.forEachIndexed { idx, label ->
                        DropdownMenuItem(
                            text = { Text(label) },
                            onClick = {
                                onSelect(key, idx)
                                menuOpen = false
                            },
                        )
                    }
                }
            }
        }
    }
}
