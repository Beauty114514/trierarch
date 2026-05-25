package app.trierarch.ui.drawer.menu

import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

internal val LocalDrawerHierarchyLevel = compositionLocalOf { 0 }

private fun normalizeLevel(level: Int): Int = level.coerceAtLeast(0)

@Composable
internal fun drawerStartPadding(level: Int = LocalDrawerHierarchyLevel.current): Dp =
    (12 + normalizeLevel(level) * 12).dp

@Composable
internal fun drawerEndPadding(): Dp = 12.dp

@Composable
internal fun drawerSectionVerticalPadding(level: Int = LocalDrawerHierarchyLevel.current): Dp =
    if (normalizeLevel(level) == 0) 10.dp else 8.dp

@Composable
internal fun drawerRowVerticalPadding(level: Int = LocalDrawerHierarchyLevel.current): Dp =
    if (normalizeLevel(level) <= 1) 10.dp else 8.dp

@Composable
internal fun drawerSectionTextStyle(level: Int = LocalDrawerHierarchyLevel.current): TextStyle =
    when (normalizeLevel(level)) {
        0 -> MaterialTheme.typography.titleMedium
        1 -> MaterialTheme.typography.bodyLarge
        else -> MaterialTheme.typography.bodyMedium
    }

@Composable
internal fun drawerRowTextStyle(level: Int = LocalDrawerHierarchyLevel.current): TextStyle =
    when (normalizeLevel(level)) {
        0, 1 -> MaterialTheme.typography.bodyLarge
        else -> MaterialTheme.typography.bodyMedium
    }
